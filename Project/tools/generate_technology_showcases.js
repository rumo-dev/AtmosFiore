// Rebuild the standalone glTF exhibits and deterministic comparison profiles.
// Uses only Node.js built-ins; no external assets or downloads are needed.
const fs = require('fs');
const path = require('path');
const root = path.resolve(__dirname, '..');
const out = path.join(root, 'data/showcases');
fs.mkdirSync(out, { recursive: true });
const white = { name: 'Matte white', pbrMetallicRoughness: {baseColorFactor:[0.65,0.65,0.65,1],metallicFactor:0,roughnessFactor:0.8}, doubleSided:true };
function material(name, color, metal=0, rough=0.7, emission=[0,0,0]) {
  const strength=Math.max(1,...emission);
  return {name,pbrMetallicRoughness:{baseColorFactor:[...color,1],metallicFactor:metal,roughnessFactor:rough},emissiveFactor:emission.map(v=>v/strength),extensions:{KHR_materials_emissive_strength:{emissiveStrength:strength}},doubleSided:true};
}
function geometry(layout, exhibit='') {
  const mats=[white,material('Red',[0.8,0.03,0.02]),material('Blue',[0.02,0.08,0.8]),material('Mirror',[0.85,0.85,0.85],1,0.06),material('Warm emitter',[1,0.4,0.08],0,0.5,[12,4,0.5]),material('Cyan emitter',[0.05,0.7,1],0,0.5,[0.4,6,12])];
  const parts=[];let sphereCount=0;
  function quad(p,n,m=0){let a=p[0],b=p[1],c=p[2],u=b.map((v,i)=>v-a[i]),v=c.map((v,i)=>v-a[i]);let cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]];parts.push({p,n:p.map(()=>n),uv:[[0,0],[1,0],[1,1],[0,1]],idx:cross.reduce((s,v,i)=>s+v*n[i],0)>0?[0,1,2,0,2,3]:[0,2,1,0,3,2],m});}
  function box(x,y,z,w,h,d,m=0){let l=x-w/2,r=x+w/2,b=y,t=y+h,f=z-d/2,k=z+d/2;quad([[l,b,f],[r,b,f],[r,t,f],[l,t,f]],[0,0,-1],m);quad([[l,b,k],[r,b,k],[r,t,k],[l,t,k]],[0,0,1],m);quad([[l,b,f],[l,b,k],[l,t,k],[l,t,f]],[-1,0,0],m);quad([[r,b,f],[r,b,k],[r,t,k],[r,t,f]],[1,0,0],m);quad([[l,t,f],[r,t,f],[r,t,k],[l,t,k]],[0,1,0],m);quad([[l,b,f],[r,b,f],[r,b,k],[l,b,k]],[0,-1,0],m);}
  function sphere(x,y,z,r,m){sphereCount++;let p=[],n=[],uv=[],idx=[];for(let j=0;j<=12;j++)for(let i=0;i<=24;i++){let a=j*Math.PI/12,b=i*2*Math.PI/24,q=[Math.sin(a)*Math.cos(b),Math.cos(a),Math.sin(a)*Math.sin(b)];p.push(q.map((v,k)=>v*r+[x,y,z][k]));n.push(q);uv.push([i/24,j/12]);}for(let j=0;j<12;j++)for(let i=0;i<24;i++){let k=j*25+i;idx.push(k,k+25,k+1,k+1,k+25,k+26);} // Fix winding to outward.
    for(let i=0;i<idx.length;i+=3){let a=p[idx[i]],b=p[idx[i+1]],c=p[idx[i+2]],u=b.map((v,k)=>v-a[k]),v=c.map((v,k)=>v-a[k]);if((u[1]*v[2]-u[2]*v[1])*n[idx[i]][0]+(u[2]*v[0]-u[0]*v[2])*n[idx[i]][1]+(u[0]*v[1]-u[1]*v[0])*n[idx[i]][2]<0)[idx[i+1],idx[i+2]]=[idx[i+2],idx[i+1]];}
    parts.push({p,n,uv,idx,m});}
  const corridor=['fog','height','distance','exponential','focus','temporal','culling'].includes(layout);
  if(layout!=='actor')box(0,-0.15,corridor?12:1,14,0.15,corridor?40:16,layout==='reflection'?3:0);
  if(corridor){for(let i=0;i<10;i++){let z=i*3;box(-3.5,0,z,0.3,3.8,0.3,i%2?1:2);box(3.5,0,z,0.3,3.8,0.3,i%2?2:1);if(layout==='fog')box(0,3.7,z,7,0.2,0.4);if(layout==='focus')sphere(i%2?1.1:-1.1,0.7,z,0.7,i%2?3:1);}
    if(layout==='temporal')for(let i=-40;i<=40;i++)box(i*0.15,0,1.3,0.025,3.3,0.025,3);
    if(layout==='culling'){box(0,0,0,5.6,1.8,0.7);for(let z=0;z<16;z++)for(let x=-6;x<=6;x++)box(x*0.45,0,z*0.6+2,0.26,1+(z%4)*0.25,0.26,(z+x+60)%3);}
  } else if(layout==='shadow'){for(let i=0;i<6;i++)box(-3+i*1.2,0,1+i*0.25,0.25,0.7+i*0.5,0.25);for(let i=0;i<5;i++)box(0,3.5,0.3+i,6,0.1,0.12);box(-4,0,2,0.2,5,8);}
  else if(layout==='ao'){box(0,0,4,10,4,0.2);box(-4,0,0,0.2,4,8);for(let i=0;i<6;i++)box(-2.5+i,0,1+(i%2)*0.6,0.8,0.3+i*0.25,0.8);}
  else if(layout==='gi'){box(-3,0,1,0.2,4,8,1);box(3,0,1,0.2,4,8,2);box(0,0,4.8,6,4,0.2);box(0,0,1.7,0.2,2.5,2);box(-2.1,1,4.6,1.4,1.8,0.05,4);box(2.1,1,4.6,1.4,1.8,0.05,5);sphere(-1.4,0.7,1,0.7,0);sphere(1.4,0.7,1,0.7,0);}
  else if(layout==='actor'){box(0,0,0,0.7,0.35,0.6,3);box(0,0.1,0,1.9,0.12,0.15,1);box(0,0.1,0,0.15,0.12,1.9,2);for(let x of [-0.8,0.8])for(let z of [-0.8,0.8]){box(x,0.25,z,0.85,0.04,0.1,3);}sphere(0,0.15,-0.32,0.13,4);}
  else if(layout==='animation'){box(0,0,0,3,0.3,3,3);box(0,0,4,8,4,0.1);}
  else if(layout==='hdr'){box(0,0,5,12,5,0.2);for(let i=0;i<7;i++){let e=Math.pow(2,i-2);let m=mats.push(material('HDR '+i,[0.2,0.2,0.2],0,0.4,[e,e*0.55,e*0.1]))-1;box((i-3)*1.2,0.6,1,0.65,1.4,0.2,m);}sphere(-2,0.5,-2,0.5,3);sphere(2,0.5,-2,0.5,3);}
  else {
    if(exhibit==='pbr') {
      // Only the material comparison needs the full metal/roughness chart.
      for(let row=0;row<3;row++)for(let col=0;col<7;col++){let m=mats.push(material('Metal / roughness '+row+' '+col,row===2?[0.8,0.43,0.08]:[0.65,0.65,0.65],row===0?0:1,0.04+col*0.15))-1;sphere((col-3)*1.15,0.55+row*1.25,1,0.5,m);}
    } else {
      const matte=exhibit==='vignette';
      const m=mats.push(material(exhibit+' subject',[0.75,0.75,0.75],matte?0:1,matte?0.6:exhibit==='area'?0.22:0.1))-1;
      sphere(0,1.2,1,1.2,m);
    }
    if(layout==='reflection'){box(-3,0,4,1,2.5,0.5,4);box(3,0,4,1,2.5,0.5,5);}
  }
  const chunks=[],views=[],accessors=[],primitives=[];let offset=0;
  function acc(values,type,components,isIndex=false){let b=Buffer.alloc(values.length*(isIndex?2:4));values.forEach((v,i)=>isIndex?b.writeUInt16LE(v,i*2):b.writeFloatLE(v,i*4));let pad=(4-offset%4)%4;if(pad){chunks.push(Buffer.alloc(pad));offset+=pad;}let view=views.push({buffer:0,byteOffset:offset,byteLength:b.length,target:isIndex?34963:34962})-1;chunks.push(b);offset+=b.length;let a={bufferView:view,componentType:isIndex?5123:5126,count:values.length/components,type};if(type==='VEC3'){a.min=Array.from({length:3},(_,k)=>Math.min(...values.filter((_,i)=>i%3===k)));a.max=Array.from({length:3},(_,k)=>Math.max(...values.filter((_,i)=>i%3===k)));}return accessors.push(a)-1;}
  // One primitive per material lets the renderer form spatial meshlets.
  for(let m=0;m<mats.length;m++){let p=[],n=[],uv=[],idx=[];for(let q of parts.filter(q=>q.m===m)){let base=p.length/3;p.push(...q.p.flat());n.push(...q.n.flat());uv.push(...q.uv.flat());idx.push(...q.idx.map(i=>i+base));}if(!p.length)continue;primitives.push({attributes:{POSITION:acc(p,'VEC3',3),NORMAL:acc(n,'VEC3',3),TEXCOORD_0:acc(uv,'VEC2',2)},indices:acc(idx,'SCALAR',1,true),material:m});}
  let animations;
  if(layout==='actor'){
    const time=acc([0,1,2,3,4],'SCALAR',1);accessors[time].min=[0];accessors[time].max=[4];
    const translation=acc([0,0,0, 0,0.3,0, 0,0,0, 0,-0.2,0, 0,0,0],'VEC3',3);
    const rotation=acc([0,0,0,1, 0,0.3826834,0,0.9238795, 0,0,0,1, 0,-0.3826834,0,0.9238795, 0,0,0,1],'VEC4',4);
    animations=[{name:'Generated flight loop',samplers:[{input:time,output:translation,interpolation:'LINEAR'},{input:time,output:rotation,interpolation:'LINEAR'}],channels:[{sampler:0,target:{node:0,path:'translation'}},{sampler:1,target:{node:0,path:'rotation'}}]}];
  }
  let buffer=Buffer.concat(chunks);return {asset:{version:'2.0',generator:'AtmosFiore technology showcases'},extras:{sphereCount,exhibit,layout},extensionsUsed:['KHR_materials_emissive_strength'],scene:0,scenes:[{nodes:[0]}],nodes:[{mesh:0,name:'Exhibit'}],meshes:[{primitives}],materials:mats,animations,buffers:[{uri:'data:application/octet-stream;base64,'+buffer.toString('base64'),byteLength:buffer.length}],bufferViews:views,accessors};
}
const definitions=[
 ['pbr','Materials','PBR metal / roughness','gallery','Compare dielectric, silver and gold rows; roughness increases to the right.'],
 ['ibl','Materials','Image based lighting','gallery','Inspect environment illumination and reflections on one polished metal sphere with no direct light.'],
 ['ssao','Lighting','SSAO contact occlusion','ao','Compare box contacts and the wall-floor corner.'],
 ['ssr','Reflections','Screen space reflections','reflection','The polished floor reflects the visible warm and cyan objects. Move sideways to see screen-space limits.'],
 ['reflection_probes','Reflections','Local reflection probes','reflection','A local cubemap captures this generated gallery. Inspect off-screen objects reflected by one polished metal sphere. SSR is disabled.'],
 ['ssgi','GI','Screen space GI','gi','Inspect red and blue diffuse transfer onto the white floor and spheres.'],
 ['voxel','GI','Voxel GI','gi','Emission lights the floor and partition, including surfaces outside the screen.'],
 ['ddgi','GI','DDGI surface probes','gi','Probe interpolation carries indirect light behind the white partition.'],
 ['ddgi_fog','GI','DDGI illuminated fog','gi','Compare warm and cyan light scattered in fog; only GI illuminates this fog view.'],
 ['sun_shadow','Shadows','Directional PCSS','shadow','Compare PCSS against 3x3 PCF. Tall pillars have wider penumbrae; overhead beams remain casters.'],
 ['point_shadow','Shadows','Point light shadows','shadow','A moving point light casts pillar shadows on the floor and walls.'],
 ['area','Lighting','Area lights','gallery','A large rectangular emitter produces a broad rectangular highlight on one moderately rough metal sphere.'],
 ['spot','Lighting','Spot lights','ao','Warm and cool cones reveal inner/outer cone falloff on the floor.'],
 ['volumetric','Atmosphere','Froxel volumetric fog','fog','Beams and pillars interrupt light shafts through the fog.'],
 ['height','Atmosphere','Height fog','height','Low fog hides pillar feet while upper portions stay clear.'],
 ['distance','Atmosphere','Distance fog','distance','Repeated pillars at known intervals fade between the start and end distance.'],
 ['exponential','Atmosphere','Exponential fog','exponential','The long corridor demonstrates progressive exponential extinction.'],
 ['bloom','HDR and camera','HDR bloom','hdr','The emissive intensity ramp blooms without blurring unlit geometry.'],
 ['exposure','HDR and camera','Physical exposure','hdr','Change aperture, shutter and ISO in the camera panel; compare exposure on/off.'],
 ['adaptation','HDR and camera','Eye adaptation','hdr','Animated illumination alternates bright and dark; observe adaptation lag.'],
 ['tone','HDR and camera','Tone mapping','hdr','Compare the HDR intensity ramp with tone mapping and linear clipping.'],
 ['dof','HDR and camera','Depth of field','focus','The middle spheres are in focus. Near and distant spheres blur; change focus distance.'],
 ['chromatic','Lens','Chromatic aberration','temporal','Inspect color separation on thin high-contrast lines at the frame edges.'],
 ['distortion','Lens','Lens distortion','temporal','Straight columns bend with lens distortion. Compare the same camera on/off.'],
 ['vignette','Lens','Vignetting','gallery','Compare center and corner brightness while keeping scene illumination fixed.'],
 ['taa','Reconstruction','Temporal anti-aliasing','temporal','Thin bars and a moving object expose shimmer and temporal history behavior.'],
 ['dlss','Reconstruction','DLSS reconstruction','temporal','Compare native TAA and DLSS Quality on the same fine geometry. Requires supported NVIDIA hardware.'],
 ['frustum','Performance','Frustum culling','culling','Seven separate courts span the camera frustum. Compare rejected instances while turning the camera. Meshlet and occlusion culling are disabled.'],
 ['meshlets','Performance','Spatial meshlet culling','culling','Turn away from parts of the dense court. Inspect skipped triangles and draw ranges; occlusion queries are disabled.'],
 ['occlusion','Performance','Hardware occlusion queries','culling','Stand still behind the large occluder for queries to settle, then move around it. Compare queries and skipped triangle counts.'],
 ['sky','Atmosphere','Dynamic sky / sun','shadow','Animated time of day changes sun direction and sky color.'],
 ['animation','Animation','glTF node animation','animation','A generated rotorcraft plays its generated translation/rotation animation above a plinth. Pause motion to compare.']
];
let baseline=JSON.parse(fs.readFileSync(path.join(root,'data/post_process_indoor.json')));
baseline.enabled=Array(13).fill(false);[0,8,12].forEach(i=>baseline.enabled[i]=true);
baseline.ssao.enabled=false;baseline.ssr.enabled=false;baseline.ssgi.enabled=false;baseline.taa.enabled=true;
baseline.exposure.reference_ev100=9.9;baseline.dof.blur_strength=1.5;baseline.bloom.intensity=0.25;
baseline.chromatic_aberration.intensity=0.65;baseline.lens_distortion.scale=0.22;baseline.vignetting.intensity=0.75;
baseline.volumetric_fog={...baseline.volumetric_fog,density:0.045,intensity:0.7,anisotropy:0.25,scattering:0.85,fog_far:40,gi_scattering:3};
baseline.height_fog={...baseline.height_fog,base_height:0.6,falloff:1.3,density:0.3,noise_strength:0};
baseline.distance_fog={...baseline.distance_fog,start:6,end:24,density:0.9,color:[0.15,0.35,0.55]};baseline.exponential_fog.density=0.055;
baseline.ssr={...baseline.ssr,reflection_strength:0.85,max_distance:25,max_steps:64};baseline.ssgi.intensity=0.55;
const effects={volumetric:3,ddgi_fog:3,height:4,distance:5,exponential:6,dof:7,exposure:8,chromatic:9,distortion:10,vignette:11,bloom:1,adaptation:2,tone:12};
const catalog=[];
for(const [id,category,title,layout,description] of definitions){
 let a=structuredClone(baseline);if(effects[id]!=null)a.enabled[effects[id]]=true;if(['ssao','ssr','ssgi'].includes(id))a[id].enabled=true;
 if(id==='dof')a.exposure.reference_ev100=4.88;
 if(id==='exposure')a.exposure.reference_ev100=8.4;
 if(id==='bloom'){a.bloom.intensity=0.9;a.bloom.threshold=0.8;}
 if(id==='chromatic')a.chromatic_aberration.intensity=3;
 if(['point_shadow','area','spot','bloom','sun_shadow'].includes(id))a.enabled[0]=false;
 let b=structuredClone(a);if(effects[id]!=null)b.enabled[effects[id]]=false;if(['ssao','ssr','ssgi'].includes(id))b[id].enabled=false;if(id==='taa')b.taa.enabled=false;
 if(id==='ddgi_fog'){b.enabled[3]=true;b.volumetric_fog.gi_scattering=0;}
 fs.writeFileSync(path.join(out,id+'.json'),JSON.stringify(a,null,2)+'\n');fs.writeFileSync(path.join(out,id+'_off.json'),JSON.stringify(b,null,2)+'\n');
 fs.writeFileSync(path.join(out,id+'.gltf'),JSON.stringify(geometry(layout,id))+'\n');
 const gi=['voxel','ddgi','ddgi_fog'].includes(id), long=['fog','height','distance','exponential','focus','temporal','culling'].includes(layout);
 catalog.push({id,category,title,description,model:'data/showcases/'+id+'.gltf',profile:'data/showcases/'+id+'.json',off_profile:'data/showcases/'+id+'_off.json',camera:[0,long?2:3.2,long?-8:-11],target:[0,long?1.6:1.8,long?8:1],sun:gi?80:['point_shadow','area','spot'].includes(id)?40:6500,gi,focus:id==='dof'?13:9});
 catalog.at(-1).environment=!(gi||['ssgi','point_shadow','area','spot','sun_shadow','bloom'].includes(id));
 if(['point_shadow','area','spot','bloom'].includes(id))catalog.at(-1).sun=300;
 if(['height','distance','exponential','volumetric'].includes(id))catalog.at(-1).sun=2000;
 if(['point_shadow','sun_shadow'].includes(id)){catalog.at(-1).camera=[0,2.8,-8];catalog.at(-1).target=[0,1,1];}
 if(['frustum','meshlets','occlusion'].includes(id)){catalog.at(-1).camera=[2.4,3.8,-8];catalog.at(-1).target=[0,1,5];}
 if(id==='animation'){catalog.at(-1).camera=[0,2.4,-5];catalog.at(-1).target=[0,1.5,0];}
 if(['ibl','ssr','reflection_probes','area','vignette'].includes(id)){catalog.at(-1).camera=[0,2.5,-7];catalog.at(-1).target=[0,1.2,1];catalog.at(-1).focus=8;}
}
// Keep separately maintained, downloaded Khronos exhibits when regenerating
// the procedural technology models.
const catalogPath=path.join(out,'catalog.json');
if(fs.existsSync(catalogPath))catalog.push(...JSON.parse(fs.readFileSync(catalogPath)).filter(e=>e.pbr_asset));
fs.writeFileSync(catalogPath,JSON.stringify(catalog,null,2)+'\n');
fs.writeFileSync(path.join(out,'actor.gltf'),JSON.stringify(geometry('actor'))+'\n');
console.log('Generated '+catalog.length+' technology exhibits and ON/OFF profiles.');
