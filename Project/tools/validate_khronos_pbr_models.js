const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert/strict');
const root=path.resolve(__dirname,'..'),manifest=JSON.parse(fs.readFileSync(path.join(root,'data/gltf_pbr/manifest.json')));
const catalog=JSON.parse(fs.readFileSync(path.join(root,'data/showcases/catalog.json'))).filter(e=>e.pbr_asset);
assert.equal(manifest.sourcePage,'https://www.khronos.org/gltf/pbr/');assert.equal(manifest.features.length,17);assert.equal(catalog.length,17);
function glb(file){const b=fs.readFileSync(file);assert.equal(b.readUInt32LE(0),0x46546c67);assert.equal(b.readUInt32LE(8),b.length);const n=b.readUInt32LE(12);return JSON.parse(b.subarray(20,20+n).toString());}
const checked=new Set();let materials=0;
for(const feature of manifest.features){
 assert(feature.source.includes('/Models/'+feature.name+'/'));assert(feature.files.some(f=>/LICENSE/i.test(f.path)),'Missing upstream license');
 for(const file of feature.files)if(!checked.has(file.path)){const b=fs.readFileSync(path.join(root,file.path));assert.equal(b.length,file.bytes);assert.equal(crypto.createHash('sha256').update(b).digest('hex'),file.sha256,'Modified downloaded file: '+file.path);checked.add(file.path);}
 const entry=catalog.find(e=>e.pbr_feature===manifest.features.indexOf(feature)+1);assert.equal(entry.model,feature.model);
 const original=glb(path.join(root,feature.model)),missing=glb(path.join(root,entry.missing_model));assert.deepEqual(original.meshes,missing.meshes,'Omission fixture must preserve official geometry');assert.deepEqual(original.bufferViews,missing.bufferViews);materials+=original.materials.length;
 assert(fs.existsSync(path.join(root,entry.profile)));assert(fs.existsSync(path.join(root,entry.off_profile)));
}
assert(!glb(path.join(root,'data/gltf_pbr/default_material_missing.glb')).materials);
console.log(`Validated 17 page-linked scenes, ${checked.size} original files with SHA-256/licenses, ${materials} materials and omission fixture geometry.`);
