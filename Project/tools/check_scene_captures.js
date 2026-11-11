const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const root=path.resolve(__dirname,'..'),folder=path.join(root,'data/capture/screenshots');
const report=JSON.parse(fs.readFileSync(path.join(folder,'report.json')));
const catalog=JSON.parse(fs.readFileSync(path.join(root,'data/showcases/catalog.json')));
const ids=catalog.map(e=>e.id).concat('Indoor');
assert.equal(report.length,ids.length*2,'Incomplete capture run');
for(const id of ids)for(const enabled of [true,false]){
 const rows=report.filter(r=>r.id===id&&r.enabled===enabled);assert.equal(rows.length,1,'Missing/duplicate '+id);
 const r=rows[0],name=id+(enabled?'_on':'_off')+'.png';
 assert.equal(path.basename(r.image),name);assert.equal(r.imgui,false);assert.equal(r.capture_hresult,0);assert.equal(r.error,'');
 const b=fs.readFileSync(path.join(root,r.image));assert.equal(b.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
 assert(b.length>10000,'Invalid/empty PNG '+id);
 if(enabled&&r.capture==='video'){
  assert.equal(r.video_capture_hresult,0,'Video capture failed '+id);
  assert.equal(r.video_fps,30);assert.equal(r.video_frames.length,60,'Incomplete video '+id);
  for(const frame of r.video_frames){const f=fs.readFileSync(path.join(root,frame));assert.equal(f.subarray(0,8).toString('hex'),'89504e470d0a1a0a','Invalid video frame '+frame);}
  assert.equal(r.video_encode_exit_code,0,'MP4 encoding failed '+id);
  assert(fs.statSync(path.join(root,r.video)).size>10000,'Invalid MP4 '+id);
 }
 assert(r.visible_fraction>0.08&&r.mean_rgb>5&&r.mean_rgb<250,'Empty/invalid exposure '+id);
 if(!r.pbr)assert(r.geometry_fraction>0.08,'No visible scene geometry '+id);
}
const on=report.find(r=>r.id==='Indoor'&&r.enabled),off=report.find(r=>r.id==='Indoor'&&!r.enabled);
assert(report.find(r=>r.id==='pbr'&&!r.enabled).mean_absolute_on_off_difference>0.05,'PBR shading toggle did not change the image');
assert.equal(on.mode,'all_features');assert.equal(off.mode,'render_only');
assert.deepEqual(on.camera_position,off.camera_position,'Indoor camera moved between shots');
assert.deepEqual(on.camera_target,off.camera_target,'Indoor target moved between shots');
assert(off.mean_absolute_on_off_difference>0.05,'Indoor modes did not change the image');
console.log(`PASS: ${ids.length} scenes / ${report.length} PNGs; feature_on/off filenames, no ImGui, Indoor render-only/all-features.`);
console.log('Indoor:',JSON.stringify({onBrightness:on.mean_rgb,offBrightness:off.mean_rgb,difference:off.mean_absolute_on_off_difference,dlss:on.dlss_enabled}));
