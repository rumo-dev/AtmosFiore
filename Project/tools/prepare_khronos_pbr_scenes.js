const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..'),folder=path.join(root,'data/gltf_pbr');
const manifest=JSON.parse(fs.readFileSync(path.join(folder,'manifest.json')));
const baseline=JSON.parse(fs.readFileSync(path.join(root,'data/showcases/ibl.json')));
const catalogFile=path.join(root,'data/showcases/catalog.json');
const catalog=JSON.parse(fs.readFileSync(catalogFile)).filter(e=>!e.pbr_asset);
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
 if(['emissive','emissive_strength'].includes(f.id)){profile.enabled[1]=true;profile.bloom.intensity=0.3;profile.bloom.threshold=1;}
 const profilePath='data/gltf_pbr/'+f.id+'.json';fs.writeFileSync(path.join(root,profilePath),JSON.stringify(profile,null,2)+'\n');
 const {json,bin}=decode(path.join(root,f.model));for(const m of json.materials||[])omit(m,f.id);
 // A removed optional extension must also leave extensionsRequired.
 const present=new Set((json.materials||[]).flatMap(m=>Object.keys(m.extensions||{})));
 json.extensionsRequired=(json.extensionsRequired||[]).filter(e=>!e.startsWith('KHR_materials_')||present.has(e));
 const missing='data/gltf_pbr/'+f.id+'_missing.glb';fs.writeFileSync(path.join(root,missing),encode(json,bin));
 catalog.push({id,category:i<7?'glTF PBR / Core':'glTF PBR / Extensions',title:'glTF: '+f.title,description:f.title+' — '+f.name+'. Official comparison asset from the Khronos PBR page. Toggle the feature without changing the camera.',model:f.model,missing_model:missing,profile:profilePath,off_profile:profilePath,camera:[0,2,-8],target:[0,1,0],sun:3000,gi:false,environment:true,focus:8,pbr_asset:true,pbr_feature:i+1,source:f.source});
}
// Entire material omitted: the official geometry must still render with defaults.
const source=decode(path.join(root,manifest.features.find(f=>f.id==='metallic').model));delete source.json.materials;for(const mesh of source.json.meshes)for(const p of mesh.primitives)delete p.material;
source.json.extensionsRequired=(source.json.extensionsRequired||[]).filter(e=>!e.startsWith('KHR_materials_'));
fs.writeFileSync(path.join(folder,'default_material_missing.glb'),encode(source.json,source.bin));
fs.writeFileSync(catalogFile,JSON.stringify(catalog,null,2)+'\n');console.log('Prepared 17 official PBR scenes, 17 omission fixtures and a material-free fixture.');
