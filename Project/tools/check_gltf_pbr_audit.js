const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const root=path.resolve(__dirname,'..');
const manifest=JSON.parse(fs.readFileSync(path.join(root,'data/gltf_pbr/manifest.json')));
const report=JSON.parse(fs.readFileSync(path.join(root,'data/gltf_pbr/audit/report.json')));
const ids=manifest.features.flatMap(f=>['gltf_pbr_'+f.id,'gltf_pbr_'+f.id+'_missing']).concat('gltf_pbr_default_material');
assert.equal(report.length,ids.length*2,'Incomplete GPU audit');
for(const id of ids)for(const enabled of [true,false]){
 const r=report.find(r=>r.id===id&&r.enabled===enabled);assert(r,'Missing capture '+id);
 assert.equal(r.error,'');assert.equal(r.capture_hresult,0);
 assert(Number.isFinite(r.mean_rgb)&&r.mean_rgb>5&&r.mean_rgb<250,'Invalid exposure '+id);
 assert(r.max_rgb-r.min_rgb>20,'Flat/empty capture '+id);
 assert(r.visible_fraction>0.08,'Black capture '+id);
 assert(fs.statSync(path.join(root,r.image)).size>10000,'Invalid PNG '+id);
 // Transparent glTF comparison models can have no opaque G-buffer coverage.
 // Their final images must be reviewed along with these numeric checks.
}
for(const f of manifest.features){const id='gltf_pbr_'+f.id,r=report.find(r=>r.id===id&&!r.enabled);
 assert(r.mean_absolute_on_off_difference>0.01,'Feature has no measurable effect: '+id);
}
console.table(manifest.features.map(f=>{const id='gltf_pbr_'+f.id,r=report.find(r=>r.id===id&&!r.enabled);return {feature:f.title,difference:r.mean_absolute_on_off_difference.toFixed(4)};}));
console.log('PASS: 17 official scenes, 17 parameter-omission cases and one material-free case; 70 GPU captures.');
