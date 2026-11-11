const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..'),folder=path.join(root,'data/gltf_pbr');
const manifest=JSON.parse(fs.readFileSync(path.join(folder,'manifest.json')));
const baseline=JSON.parse(fs.readFileSync(path.join(root,'data/showcases/ibl.json')));
const catalogFile=path.join(root,'data/showcases/catalog.json');
const catalog=JSON.parse(fs.readFileSync(catalogFile)).filter(e=>!e.pbr_asset);
// Keep the downloaded comparison assets intact. The site slider needs one
// specimen per image, so display copies reference only the right-hand roots.
const displayRoots={alpha:[6],occlusion:[0,1,2],base_color:[2],emissive:[1],metallic:[1],normal:[1],roughness:[1],anisotropy:[0],clearcoat:[2],dispersion:[0,2],emissive_strength:[1],ior:[1,2],iridescence:[1],sheen:[1],specular:[1],transmission:[3,6],volume:[3,6]};
function displayBounds(gltf,roots){
 const identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];
 function multiply(a,b){let o=Array(16).fill(0);for(let c=0;c<4;c++)for(let r=0;r<4;r++)for(let k=0;k<4;k++)o[c*4+r]+=a[k*4+r]*b[c*4+k];return o;}
 function local(n){if(n.matrix)return n.matrix;const [x,y,z,w]=n.rotation||[0,0,0,1],s=n.scale||[1,1,1],t=n.translation||[0,0,0];return [(1-2*(y*y+z*z))*s[0],2*(x*y+z*w)*s[0],2*(x*z-y*w)*s[0],0,2*(x*y-z*w)*s[1],(1-2*(x*x+z*z))*s[1],2*(y*z+x*w)*s[1],0,2*(x*z+y*w)*s[2],2*(y*z-x*w)*s[2],(1-2*(x*x+y*y))*s[2],0,...t,1];}
 const min=[Infinity,Infinity,Infinity],max=[-Infinity,-Infinity,-Infinity];
 function visit(i,parent){const n=gltf.nodes[i],m=multiply(parent,local(n));if(n.mesh!=null)for(const primitive of gltf.meshes[n.mesh].primitives){const a=gltf.accessors[primitive.attributes.POSITION];if(!a.min||!a.max)throw Error('Missing POSITION bounds');for(let mask=0;mask<8;mask++){const p=[0,1,2].map(k=>mask&(1<<k)?a.max[k]:a.min[k]);const q=[m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]];for(let k=0;k<3;k++){min[k]=Math.min(min[k],q[k]);max[k]=Math.max(max[k],q[k]);}}}for(const child of n.children||[])visit(child,m);}
 for(const i of roots.filter(i=>!gltf.nodes[i].name?.toLowerCase().includes('checker')))visit(i,identity);
 if(!min.every(Number.isFinite)||!max.every(Number.isFinite))throw Error('Invalid display bounds');
 return {min:[min[0],min[1],-max[2]],max:[max[0],max[1],-min[2]]};
}
function decode(file){const b=fs.readFileSync(file);if(b.readUInt32LE(0)!==0x46546c67)throw Error('Expected downloaded GLB');let json,bin;for(let i=12;i<b.length;){const n=b.readUInt32LE(i),kind=b.readUInt32LE(i+4);if(kind===0x4e4f534a)json=JSON.parse(b.subarray(i+8,i+8+n).toString());if(kind===0x004e4942)bin=b.subarray(i+8,i+8+n);i+=8+n;}return {json,bin};}
function encode(json,bin){let j=Buffer.from(JSON.stringify(json));j=Buffer.concat([j,Buffer.alloc((4-j.length%4)%4,32)]);let b=Buffer.concat([bin,Buffer.alloc((4-bin.length%4)%4)]);const out=Buffer.alloc(12+8+j.length+8+b.length);out.writeUInt32LE(0x46546c67);out.writeUInt32LE(2,4);out.writeUInt32LE(out.length,8);out.writeUInt32LE(j.length,12);out.writeUInt32LE(0x4e4f534a,16);j.copy(out,20);out.writeUInt32LE(b.length,20+j.length);out.writeUInt32LE(0x004e4942,24+j.length);b.copy(out,28+j.length);return out;}
function omit(m,id){const p=m.pbrMetallicRoughness||{};
 const fields={alpha:['alphaMode','alphaCutoff'],occlusion:['occlusionTexture'],base_color:['pbrMetallicRoughness.baseColorFactor','pbrMetallicRoughness.baseColorTexture'],emissive:['emissiveFactor','emissiveTexture'],metallic:['pbrMetallicRoughness.metallicFactor'],normal:['normalTexture'],roughness:['pbrMetallicRoughness.roughnessFactor']};
 for(const field of fields[id]||[]){const [a,b]=field.split('.');if(b)delete p[b];else delete m[a];}
 const extensions={anisotropy:'anisotropy',clearcoat:'clearcoat',dispersion:'dispersion',emissive_strength:'emissive_strength',ior:'ior',iridescence:'iridescence',sheen:'sheen',specular:'specular',transmission:'transmission',volume:'volume'};
 if(extensions[id]&&m.extensions)delete m.extensions['KHR_materials_'+extensions[id]];
}
for(let i=0;i<manifest.features.length;i++){
 const f=manifest.features[i],id='gltf_pbr_'+f.id,profile=structuredClone(baseline);profile.ssao.enabled=false;profile.ssr.enabled=false;profile.exposure.reference_ev100=8.9;
 if(['emissive','emissive_strength'].includes(f.id)){profile.enabled[1]=true;profile.bloom.intensity=0.7;profile.bloom.threshold=0.8;}
 const profilePath='data/gltf_pbr/'+f.id+'.json';fs.writeFileSync(path.join(root,profilePath),JSON.stringify(profile,null,2)+'\n');
 const {json,bin}=decode(path.join(root,f.model));for(const m of json.materials||[])omit(m,f.id);
 const display=decode(path.join(root,f.model));const roots=displayRoots[f.id];if(!roots||!roots.every(i=>display.json.scenes[display.json.scene||0].nodes.includes(i)))throw Error('Display roots changed: '+f.id);
 display.json.scenes[display.json.scene||0].nodes=roots;
 const bounds=displayBounds(display.json,roots);
 const displayModel='data/gltf_pbr/'+f.name+'/glTF-Binary/'+f.name+'Right.glb';fs.writeFileSync(path.join(root,displayModel),encode(display.json,display.bin));
 // A removed optional extension must also leave extensionsRequired.
 const present=new Set((json.materials||[]).flatMap(m=>Object.keys(m.extensions||{})));
 json.extensionsRequired=(json.extensionsRequired||[]).filter(e=>!e.startsWith('KHR_materials_')||present.has(e));
 const missing='data/gltf_pbr/'+f.id+'_missing.glb';fs.writeFileSync(path.join(root,missing),encode(json,bin));
 catalog.push({id,category:i<7?'glTF PBR / Core':'glTF PBR / Extensions',title:'glTF: '+f.title,description:f.title+' — '+f.name+'. Official comparison asset from the Khronos PBR page. Toggle the feature without changing the camera.',model:f.model,display_model:displayModel,display_bounds:bounds,missing_model:missing,profile:profilePath,off_profile:profilePath,camera:[0,2,-8],target:[0,1,0],sun:3000,gi:false,environment:true,focus:8,pbr_asset:true,pbr_feature:i+1,capture:'slider',source:f.source});
}
// Entire material omitted: the official geometry must still render with defaults.
const source=decode(path.join(root,manifest.features.find(f=>f.id==='metallic').model));delete source.json.materials;for(const mesh of source.json.meshes)for(const p of mesh.primitives)delete p.material;
source.json.extensionsRequired=(source.json.extensionsRequired||[]).filter(e=>!e.startsWith('KHR_materials_'));
fs.writeFileSync(path.join(folder,'default_material_missing.glb'),encode(source.json,source.bin));
fs.writeFileSync(catalogFile,JSON.stringify(catalog,null,2)+'\n');console.log('Prepared 17 official PBR scenes, 17 omission fixtures and a material-free fixture.');
