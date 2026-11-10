#include "gltf_model.hlsli"
#include "gltf_pbr_data.hlsli"
#include "point_light.hlsli"
#include "spot_light.hlsli"
#include "ddgi_sample.hlsli"

TextureCube<float4> pbr_diffuse_environment : register(t33);
TextureCube<float4> pbr_specular_environment : register(t34);
Texture2D<float4> pbr_brdf_lut : register(t35);
TextureCube<float4> pbr_probe0 : register(t36);
TextureCube<float4> pbr_probe1 : register(t37);
TextureCube<float4> pbr_probe2 : register(t38);
cbuffer PBR_LOCAL_PROBES : register(b10) { float4 pbr_probe_positions[3];float4 pbr_probe_min[3];float4 pbr_probe_max[3];float4 pbr_probe_settings; };
struct PbrAreaLight { float3 position;float width;float3 direction;float height;float3 right;float radius;uint shape;float intensity;float range;uint padding;float3 color;float padding2; };
StructuredBuffer<PbrAreaLight> pbr_area_lights : register(t10);
cbuffer PBR_AREA_COUNT : register(b7) {uint pbr_area_count;float3 pbr_area_padding;};
Texture3D<float4> pbr_voxel_radiance : register(t13);
cbuffer PBR_VOXEL_CONSTANTS : register(b8) {float4 pbr_voxel_min;float4 pbr_voxel_inv_extent;uint pbr_voxel_resolution;uint pbr_voxel_slice;float pbr_voxel_strength;float pbr_voxel_emission;float pbr_voxel_environment;float3 pbr_voxel_padding;};
Texture2D<float> pbr_ssao : register(t40);
Texture2D<float> pbr_shadow_mask : register(t41);
Texture2D<float4> pbr_background : register(t43);
Texture2D<float4> pbr_background_normal : register(t44);
static const float PBR_PI=3.141592653589793f;
static float3 pbr_surface_position=0;

float3 PbrEnvironment(float3 direction,float roughness)
{
    uint w=0,h=0,levels=1;pbr_specular_environment.GetDimensions(0,w,h,levels);
    float3 global=pbr_specular_environment.SampleLevel(pbr_environment_sampler,direction,saturate(roughness)*max(float(levels)-1,0)).rgb;
    float3 local=0;float total=0;
    [unroll]for(uint i=0;i<3;++i){if(i>=(uint)pbr_probe_settings.x)break;float3 edge=min(pbr_surface_position-pbr_probe_min[i].xyz,pbr_probe_max[i].xyz-pbr_surface_position);if(any(edge<0))continue;
        float weight=saturate(min(edge.x,min(edge.y,edge.z))/max(pbr_probe_settings.y,0.01));
        float3 safe=(step(0,direction)*2-1)*max(abs(direction),1e-5);float3 plane=lerp(pbr_probe_min[i].xyz,pbr_probe_max[i].xyz,step(0,direction));float3 travel=(plane-pbr_surface_position)/safe;
        float3 corrected=pbr_surface_position+direction*min(travel.x,min(travel.y,travel.z))-pbr_probe_positions[i].xyz;float lod=saturate(roughness)*7;
        float3 sampleColor=i==0?pbr_probe0.SampleLevel(pbr_environment_sampler,corrected,lod).rgb:i==1?pbr_probe1.SampleLevel(pbr_environment_sampler,corrected,lod).rgb:pbr_probe2.SampleLevel(pbr_environment_sampler,corrected,lod).rgb;local+=sampleColor*weight;total+=weight;}
    return total>0?lerp(global,local/max(total,1e-5)*pbr_probe_settings.z,saturate(total)):global;
}
float3 PbrFresnel(float3 f0,float3 f90,float cosine) { return f0+(f90-f0)*pow(1-saturate(cosine),5); }
float PbrD(float nh,float alpha) { float a2=alpha*alpha,d=nh*nh*(a2-1)+1;return a2/max(PBR_PI*d*d,1e-8); }
float PbrVis(float nl,float nv,float alpha)
{ float a2=alpha*alpha;return 0.5f/max(nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2),1e-6); }
float3x3 PbrFrame(float3 p,float3 n,float2 uv,float3 fallbackT)
{
    float3 px=ddx(p),py=ddy(p);float2 tx=ddx(uv),ty=ddy(uv);
    float determinant=tx.x*ty.y-tx.y*ty.x;
    float3 t=abs(determinant)>1e-10?(px*ty.y-py*tx.y)/determinant:fallbackT;
    t-=n*dot(n,t);if(dot(t,t)<1e-8)t=cross(abs(n.y)<0.9?float3(0,1,0):float3(1,0,0),n);
    t=normalize(t);float3 b=normalize(cross(n,t));
    if(abs(determinant)>1e-10&&dot(b,(py*tx.x-px*ty.x)/determinant)<0)b=-b;
    return float3x3(t,b,n);
}
float3 PbrNormal(float3 texel,float scale,float3x3 frame)
{ float3 n=texel*2-1;n.xy*=scale;return normalize(mul(normalize(n),frame)); }

// A two-interface thin-film Fresnel evaluated at representative RGB wavelengths.
// The optical path and phase shifts depend on both IORs, thickness and angle.
float3 PbrFilm(float cosine,float filmIor,float thickness,float3 substrateF0)
{
    if(thickness<=0)return PbrFresnel(substrateF0,1,cosine);
    float sin2=(1-cosine*cosine)/(filmIor*filmIor);if(sin2>=1)return 1;
    float filmCos=sqrt(max(1-sin2,0));
    float r01=(1-filmIor)/(1+filmIor);float R01=PbrFresnel(r01*r01,1,cosine).x;
    float3 sqrtF=sqrt(clamp(substrateF0,0,0.9999));float3 baseIor=(1+sqrtF)/max(1-sqrtF,1e-5);
    float3 r12=(filmIor-baseIor)/(filmIor+baseIor);
    float3 R12=PbrFresnel(r12*r12,1,filmCos);
    float3 phase=4*PBR_PI*filmIor*thickness*filmCos/float3(650,510,475);
    float3 interference=2*sqrt(R01*R12)*cos(phase)*sign(r01*r12);
    return saturate((R01+R12+interference)/max(1+R01*R12+interference,1e-5));
}
struct PbrSurface
{
    float3 base,N,V,T,B,f0,f90,coatN,sheen,emissive;
    float roughness,metallic,coat,coatRoughness,sheenRoughness,iridescence,filmIor,filmThickness,anisotropy;
};
float3 PbrSurfaceF(PbrSurface s,float cosine)
{
    float3 normalF=PbrFresnel(s.f0,s.f90,cosine);
    return lerp(normalF,PbrFilm(cosine,s.filmIor,s.filmThickness,s.f0),s.iridescence);
}
float3 PbrDirect(PbrSurface s,float3 L,float3 radiance)
{
    float nl=saturate(dot(s.N,L)),nv=max(saturate(dot(s.N,s.V)),1e-4);if(nl<=0)return 0;
    float3 H=normalize(L+s.V);float nh=saturate(dot(s.N,H)),vh=saturate(dot(s.V,H));
    float alpha=max(s.roughness*s.roughness,0.0016);float D=PbrD(nh,alpha),vis=PbrVis(nl,nv,alpha);
    if(s.anisotropy>0){float at=lerp(alpha,1,s.anisotropy*s.anisotropy),ab=alpha;
        float3 v=float3(dot(s.T,H)/at,dot(s.B,H)/ab,nh);D=1/max(PBR_PI*at*ab*pow(dot(v,v),2),1e-8);
        float lv=length(float3(at*dot(s.T,s.V),ab*dot(s.B,s.V),nv));float ll=length(float3(at*dot(s.T,L),ab*dot(s.B,L),nl));vis=0.5/max(nl*lv+nv*ll,1e-6);}
    float3 F=PbrSurfaceF(s,vh);float3 diffuse=(1-F)*s.base*(1-s.metallic)/PBR_PI;
    float3 specular=F*D*vis;
    float sheenAlpha=max(s.sheenRoughness*s.sheenRoughness,0.001);
    float sheenD=(2+1/sheenAlpha)*pow(max(1-nh*nh,1e-6),0.5/sheenAlpha)/(2*PBR_PI);
    float3 sheen=s.sheen*sheenD/(4*max(nl+nv-nl*nv,1e-4));
    float sheenEnergy=1-max(s.sheen.r,max(s.sheen.g,s.sheen.b))*0.5;
    float3 base=(diffuse+specular)*sheenEnergy+sheen;
    float cnl=saturate(dot(s.coatN,L)),cnv=max(saturate(dot(s.coatN,s.V)),1e-4),cnh=saturate(dot(s.coatN,H));
    float Fc=0.04+0.96*pow(1-vh,5),ca=max(s.coatRoughness*s.coatRoughness,0.0016);
    return radiance*(base*nl*(1-s.coat*Fc)+s.coat*Fc*PbrD(cnh,ca)*PbrVis(cnl,cnv,ca)*cnl);
}
float3 PbrRefraction(float3 worldPos,float3 N,float3 V,float ior,float thickness,float roughness,out float distance)
{
    distance=0;
    float3 ray=refract(-V,N,1/max(ior,1.00001));if(dot(ray,ray)<1e-8)ray=-V;
    float3 scale=float3(length(world[0].xyz),length(world[1].xyz),length(world[2].xyz));
    float3 travel=normalize(ray)*thickness*scale;distance=length(travel);
    float4 exitClip=mul(float4(worldPos+travel,1),view_projection);
    float2 uv=exitClip.xy/max(exitClip.w,1e-5)*float2(0.5,-0.5)+0.5;
    uint w=0,h=0,levels=1;pbr_background.GetDimensions(0,w,h,levels);
    if(w==0||exitClip.w<=0||any(uv<0)||any(uv>1))return PbrEnvironment(ray,roughness);
    float3 backgroundNormal=pbr_background_normal.SampleLevel(pbr_environment_sampler,uv,0).xyz;
    if(dot(backgroundNormal,backgroundNormal)<0.1)return PbrEnvironment(ray,roughness);
    return pbr_background.SampleLevel(pbr_environment_sampler,uv,saturate(roughness*(ior*2-2))*max(float(levels)-1,0)).rgb;
}
float4 main(VS_OUT pin,bool front:SV_IsFrontFace) : SV_TARGET
{
    PbrMaterial m=pbr_materials[material];int disabled=(int)m.alpha.z;
    pbr_surface_position=pin.w_position.xyz;
    float4 vertexColor=disabled==3?1:pin.color;
    float4 base=m.base*vertexColor;float4 baseTex=PbrTex(0,m.textures[0],pin.texcoord,pin.texcoord1,1);
    base*=float4(PbrLinear(baseTex.rgb),baseTex.a);if(disabled==3)base=1;
    int alphaMode=disabled==1?0:m.flags.x;if(alphaMode==0)base.a=1;else if(alphaMode==1)clip(base.a-m.alpha.x);
    if(!front&&(m.flags.y==0||m.transmission.y>0))discard;
    float3 N=pin.w_normal.xyz;if(dot(N,N)<1e-8)N=cross(ddx(pin.w_position.xyz),ddy(pin.w_position.xyz));N=normalize(N);if(!front)N=-N;
    float3x3 frame=PbrFrame(pin.w_position.xyz,N,PbrUV(m.textures[2],pin.texcoord,pin.texcoord1),pin.w_tangent.xyz);
    float3 coatN=N;
    if(m.textures[2].info.x>=0&&disabled!=6)N=PbrNormal(PbrTex(2,m.textures[2],pin.texcoord,pin.texcoord1,float4(0.5,0.5,1,1)).xyz,m.surface.z,frame);
    if(m.textures[7].info.x>=0)coatN=PbrNormal(PbrTex(7,m.textures[7],pin.texcoord,pin.texcoord1,float4(0.5,0.5,1,1)).xyz,m.coat.z,PbrFrame(pin.w_position.xyz,coatN,PbrUV(m.textures[7],pin.texcoord,pin.texcoord1),pin.w_tangent.xyz));
    float4 mr=PbrTex(1,m.textures[1],pin.texcoord,pin.texcoord1,1);
    PbrSurface s=(PbrSurface)0;s.base=base.rgb;s.N=N;s.V=normalize(camera_position.xyz-pin.w_position.xyz);
    s.roughness=disabled==7?1:saturate(m.surface.y*mr.g);s.metallic=disabled==5?0:saturate(m.surface.x*mr.b);
    float3 specColor=m.specular.rgb*PbrLinear(PbrTex(13,m.textures[13],pin.texcoord,pin.texcoord1,1).rgb);
    float specStrength=m.specular.a*PbrTex(12,m.textures[12],pin.texcoord,pin.texcoord1,1).a;
    if(disabled==15){specColor=1;specStrength=1;}
    float ior=disabled==12?1.5:m.coat.w;bool infiniteIor=ior==0;float dielectric=infiniteIor?1:pow((ior-1)/max(ior+1,1e-6),2);
    s.f0=lerp(min(dielectric*specColor,1)*specStrength,base.rgb,s.metallic);s.f90=infiniteIor?s.f0:lerp(specStrength.xxx,1,s.metallic);
    s.coat=disabled==9?0:saturate(m.coat.x*PbrTex(5,m.textures[5],pin.texcoord,pin.texcoord1,1).r);
    s.coatRoughness=saturate(m.coat.y*PbrTex(6,m.textures[6],pin.texcoord,pin.texcoord1,1).g);s.coatN=coatN;
    s.sheen=disabled==14?0:m.sheen.rgb*PbrLinear(PbrTex(10,m.textures[10],pin.texcoord,pin.texcoord1,1).rgb);
    s.sheenRoughness=m.sheen.a*PbrTex(11,m.textures[11],pin.texcoord,pin.texcoord1,1).a;
    s.iridescence=disabled==13?0:m.iridescence.x*PbrTex(15,m.textures[15],pin.texcoord,pin.texcoord1,1).r;
    s.filmIor=max(m.iridescence.y,1);s.filmThickness=m.textures[16].info.x>=0?lerp(m.iridescence.z,m.iridescence.w,PbrTex(16,m.textures[16],pin.texcoord,pin.texcoord1,1).g):m.iridescence.w;
    float3 an=PbrTex(14,m.textures[14],pin.texcoord,pin.texcoord1,float4(1,0.5,1,1)).rgb;float2 axis=an.rg*2-1;if(dot(axis,axis)<1e-8)axis=float2(1,0);axis=normalize(axis);
    float2 rot=float2(cos(m.anisotropy.y),sin(m.anisotropy.y));axis=float2(axis.x*rot.x-axis.y*rot.y,axis.x*rot.y+axis.y*rot.x);
    float3x3 anisotropyFrame=PbrFrame(pin.w_position.xyz,N,PbrUV(m.textures[14],pin.texcoord,pin.texcoord1),pin.w_tangent.xyz);
    s.anisotropy=disabled==8?0:saturate(m.anisotropy.x*an.b);s.T=normalize(anisotropyFrame[0]*axis.x+anisotropyFrame[1]*axis.y);s.T=normalize(s.T-N*dot(N,s.T));s.B=normalize(cross(N,s.T));
    float transmission=disabled==16?0:saturate(m.transmission.x*PbrTex(8,m.textures[8],pin.texcoord,pin.texcoord1,1).r);
    float thickness=disabled==17?0:m.transmission.y*PbrTex(9,m.textures[9],pin.texcoord,pin.texcoord1,1).g;
    float ao=disabled==2?1:lerp(1,PbrTex(4,m.textures[4],pin.texcoord,pin.texcoord1,1).r,m.surface.w);
    s.emissive=disabled==4?0:m.emissive.rgb*(disabled==11?1:m.emissive.a)*PbrLinear(PbrTex(3,m.textures[3],pin.texcoord,pin.texcoord1,1).rgb);
    if(m.flags.z!=0)return float4(base.rgb,base.a);
    float3 opaqueBase=s.base;s.base*=1-transmission;
    uint sw,sh;pbr_shadow_mask.GetDimensions(sw,sh);float shadow=1;
    if(sw>0){float4 clipPos=mul(float4(pin.w_position.xyz,1),view_projection);float2 screen=clipPos.xy/clipPos.w*float2(0.5,-0.5)+0.5;shadow=pbr_shadow_mask.SampleLevel(pbr_environment_sampler,screen,0);}
    float3 color=PbrDirect(s,normalize(-directional_light_direction.xyz),directional_light_color.rgb*(directional_light_color.w/683.0)*shadow);
    [loop]for(uint i=0;i<numPointLights;++i){float3 d=pointLights[i].position-pin.w_position.xyz;float distance=length(d);float fade=pow(saturate(1-pow(distance/max(pointLights[i].radius,1e-4),4)),2);color+=PbrDirect(s,d/max(distance,1e-5),pointLights[i].color*pointLights[i].intensity*fade/(683*(distance*distance+0.04)));}
    [loop]for(uint spot=0;spot<numSpotLights;++spot){float3 d=spotLights[spot].position-pin.w_position.xyz;float distance=length(d);float3 L=d/max(distance,1e-5);float cone=smoothstep(cos(spotLights[spot].outerAngle),cos(spotLights[spot].innerAngle),dot(-L,normalize(spotLights[spot].direction)));float fade=pow(saturate(1-pow(distance/max(spotLights[spot].radius,1e-4),4)),2);color+=PbrDirect(s,L,spotLights[spot].color*spotLights[spot].intensity*cone*fade/(683*(distance*distance+0.04)));}
    [loop]for(uint area=0;area<pbr_area_count;++area){PbrAreaLight light=pbr_area_lights[area];float3 toLight=light.position-pin.w_position.xyz;float d=length(toLight);float3 L=toLight/max(d,1e-5);float size=light.shape==0?light.width*light.height:light.shape==1?PBR_PI*light.radius*light.radius:4*PBR_PI*light.radius*light.radius;float facing=light.shape==2?1:saturate(dot(-L,normalize(light.direction)));float fade=pow(saturate(1-pow(d/max(light.range,1e-4),4)),2);color+=PbrDirect(s,L,light.color*light.intensity*(size*facing*fade/(683*(d*d+size))));}
    float nv=saturate(dot(N,s.V));float3 F=PbrSurfaceF(s,nv);float3 diffuseLight=pbr_diffuse_environment.SampleLevel(pbr_environment_sampler,N,0).rgb;
    float3 reflectionN=N;
    if(s.anisotropy>0){float3 bent=normalize(cross(cross(s.B,s.V),s.B));reflectionN=normalize(lerp(N,bent,s.anisotropy*pow(1-s.roughness,4)));}
    float2 lut=pbr_brdf_lut.SampleLevel(pbr_environment_sampler,float2(nv,s.roughness),0).rg;
    float3 iblDiffuse=diffuseLight*s.base*(1-s.metallic)*(1-F);
    float3 iblSpec=PbrEnvironment(reflect(-s.V,reflectionN),s.roughness)*(F*lut.x+s.f90*lut.y);
    float coatF=(0.04+0.96*pow(1-saturate(dot(coatN,s.V)),5))*s.coat;
    uint aw=0,ah=0;pbr_ssao.GetDimensions(aw,ah);float screenAO=1;if(aw>0){float4 cp=mul(float4(pin.w_position.xyz,1),view_projection);screenAO=pbr_ssao.SampleLevel(pbr_environment_sampler,cp.xy/cp.w*float2(0.5,-0.5)+0.5,0);}
    color+=(iblDiffuse+iblSpec)*(1-coatF)*ao*screenAO;
    color+=PbrEnvironment(reflect(-s.V,coatN),s.coatRoughness)*coatF*ao;
    color+=s.sheen*PbrEnvironment(reflect(-s.V,N),s.sheenRoughness)*pow(1-nv,1+2*s.sheenRoughness)*ao;
    float3 voxelGI=0;uint vw=0,vh=0,vd=0;pbr_voxel_radiance.GetDimensions(vw,vh,vd);
    if(vw>0&&pbr_voxel_strength>0){float3 origin=pin.w_position.xyz+N*pbr_voxel_inv_extent.w*1.5;float alpha=0;float d=pbr_voxel_inv_extent.w;
        [loop]for(uint stepIndex=0;stepIndex<12&&alpha<0.95;++stepIndex){float3 uvw=(origin+N*d-pbr_voxel_min.xyz)*pbr_voxel_inv_extent.xyz;if(any(uvw<0)||any(uvw>=1))break;float diameter=max(pbr_voxel_inv_extent.w,d*0.9);float4 v=pbr_voxel_radiance.SampleLevel(pbr_environment_sampler,uvw,max(0,log2(diameter/pbr_voxel_inv_extent.w)));voxelGI+=(1-alpha)*v.rgb;alpha+=(1-alpha)*v.a;d+=diameter*0.75;}voxelGI*=pbr_voxel_strength;}
    uint probeCount=0,probeStride=0;ddgi_probes.GetDimensions(probeCount,probeStride);float4 probeGI=0;if(probeCount>0)probeGI=sample_ddgi(pin.w_position.xyz,N);
    color+=lerp(voxelGI,probeGI.rgb*ddgi_strength,probeGI.a*saturate(ddgi_blend))*s.base*(1-s.metallic)*ao*screenAO;
    if(transmission>0)
    {
        float spread=disabled==10||infiniteIor?0:(ior-1)*0.025*m.transmission.w;
        float3 indices=infiniteIor?float3(1e6,1e6,1e6):max(float3(ior-spread,ior,ior+spread),1.00001);float dr=0,dg=0,db=0;
        float3 refractedR=PbrRefraction(pin.w_position.xyz,N,s.V,indices.r,thickness,s.roughness,dr);
        float3 refractedG=PbrRefraction(pin.w_position.xyz,N,s.V,indices.g,thickness,s.roughness,dg);
        float3 refractedB=PbrRefraction(pin.w_position.xyz,N,s.V,indices.b,thickness,s.roughness,db);
        float3 absorbed=exp(log(max(m.attenuation.rgb,1e-6))*float3(dr,dg,db)/max(m.transmission.z,1e-6));if(disabled==17)absorbed=1;
        color+=float3(refractedR.r,refractedG.g,refractedB.b)*absorbed*opaqueBase*(1-F)*transmission*(1-s.metallic)*(1-coatF);
    }
    return float4(max(color+s.emissive,0),alphaMode==2?base.a:1);
}
