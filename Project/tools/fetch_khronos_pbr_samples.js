// Models linked by https://www.khronos.org/gltf/pbr/ (not substitute models).
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'..'),out=path.join(root,'data/gltf_pbr');fs.mkdirSync(out,{recursive:true});
const features=[['alpha','Alpha Coverage','CompareAlphaCoverage'],['occlusion','Ambient Occlusion','CompareAmbientOcclusion'],['base_color','Base Color','CompareBaseColor'],['emissive','Emissive','CompareEmissiveStrength'],['metallic','Metallic','CompareMetallic'],['normal','Normal','CompareNormal'],['roughness','Roughness','CompareRoughness'],['anisotropy','Anisotropy','CompareAnisotropy'],['clearcoat','Clearcoat','CompareClearcoat'],['dispersion','Dispersion','CompareDispersion'],['emissive_strength','Emissive Strength','CompareEmissiveStrength'],['ior','Index of Refraction','CompareIor'],['iridescence','Iridescence','CompareIridescence'],['sheen','Sheen','CompareSheen'],['specular','Specular','CompareSpecular'],['transmission','Transmission','CompareTransmission'],['volume','Volume','CompareVolume']];
async function get(url,json=false){const r=await fetch(url,{headers:{'User-Agent':'AtmosFiore-PBR-validation','Accept':'application/vnd.github+json'}});if(!r.ok)throw Error(r.status+' '+url);return json?r.json():Buffer.from(await r.arrayBuffer());}
async function main(){
 const repo='KhronosGroup/glTF-Sample-Assets',api='https://api.github.com/repos/'+repo;
 const commit=(await get(api+'/commits/main',true)).sha;
 const tree=(await get(api+'/git/trees/'+commit+'?recursive=1',true));if(tree.truncated)throw Error('Incomplete upstream file inventory');
 const models={};
 for(const name of new Set(features.map(f=>f[2]))){
   const base='Models/'+name+'/',files=tree.tree.filter(e=>e.type==='blob'&&e.path.startsWith(base));
   const glb=files.find(e=>e.path.startsWith(base+'glTF-Binary/')&&e.path.endsWith('.glb'));
   const selected=files.filter(e=>/^([^/]+\/)*([^/]*LICENSE[^/]*|README[^/]*|metadata.json)$/i.test(e.path.slice(base.length))&&(e.path.slice(base.length).indexOf('/')<0));
   if(glb)selected.push(glb);else selected.push(...files.filter(e=>e.path.startsWith(base+'glTF/')));
   const entry=glb||selected.find(e=>e.path.endsWith('.gltf'));if(!entry)throw Error('No uncompressed asset: '+name);
   const records=[];
   // Small bounded download batches avoid fetching unrelated repository assets.
   for(let i=0;i<selected.length;i+=4)await Promise.all(selected.slice(i,i+4).map(async e=>{
     const relative=e.path.slice(base.length),destination=path.join(out,name,relative),url='https://raw.githubusercontent.com/'+repo+'/'+commit+'/'+e.path;
     const data=await get(url);fs.mkdirSync(path.dirname(destination),{recursive:true});fs.writeFileSync(destination,data);
     records.push({path:path.relative(root,destination).replaceAll('\\','/'),url,bytes:data.length,sha256:crypto.createHash('sha256').update(data).digest('hex')});
   }));
   models[name]={model:'data/gltf_pbr/'+name+'/'+entry.path.slice(base.length),source:'https://github.com/'+repo+'/tree/'+commit+'/'+base,files:records.sort((a,b)=>a.path.localeCompare(b.path))};
   console.log(name+': '+records.reduce((s,e)=>s+e.bytes,0)+' bytes');
 }
 const manifest={sourcePage:'https://www.khronos.org/gltf/pbr/',repository:'https://github.com/'+repo,commit,features:features.map(([id,title,name])=>({id,title,name,...models[name]}))};
 fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,2)+'\n');
 const extensions=['anisotropy','clearcoat','dispersion','emissive_strength','ior','iridescence','sheen','specular','transmission','volume'];
 for(const name of extensions){const url='https://raw.githubusercontent.com/KhronosGroup/glTF/main/extensions/2.0/Khronos/KHR_materials_'+name+'/README.md';const data=await get(url);const folder=path.join(out,'specs');fs.mkdirSync(folder,{recursive:true});fs.writeFileSync(path.join(folder,'KHR_materials_'+name+'.md'),data);}
 console.log('Saved '+features.length+' feature entries, pinned upstream models, credits and material specifications.');
}
main().catch(e=>{console.error(e);process.exitCode=1;});
