// Validate data consumed by the production glTF loader and showcase selector.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const root=path.resolve(__dirname,'..');
const catalog=JSON.parse(fs.readFileSync(path.join(root,'data/showcases/catalog.json')));
const ids=new Set();let triangles=0;
for(const e of catalog){
 assert(!ids.has(e.id),'Duplicate exhibit '+e.id);ids.add(e.id);
 assert(e.camera.length===3&&e.target.length===3&&e.camera.every(Number.isFinite));
 const on=JSON.parse(fs.readFileSync(path.join(root,e.profile)));
 const off=JSON.parse(fs.readFileSync(path.join(root,e.off_profile)));
 for(const p of [on,off]){assert.equal(p.version,1);assert.equal(p.enabled.length,13);assert.equal(new Set(p.order).size,13);}
 const d=JSON.parse(fs.readFileSync(path.join(root,e.model)));
 if(e.id==='pbr')assert.equal(d.extras.sphereCount,21,'PBR material chart must retain its comparison grid');
 if(['ibl','ssr','reflection_probes','area','vignette'].includes(e.id))assert.equal(d.extras.sphereCount,1,e.id+' must have one sphere');
 const b=Buffer.from(d.buffers[0].uri.split(',')[1],'base64');assert.equal(b.length,d.buffers[0].byteLength);
 const widths={SCALAR:1,VEC2:2,VEC3:3,VEC4:4};
 function read(i){const a=d.accessors[i],v=d.bufferViews[a.bufferView],bytes=a.componentType===5126?4:2,n=widths[a.type],offset=(v.byteOffset||0)+(a.byteOffset||0);assert(offset+a.count*n*bytes<=b.length);return Array.from({length:a.count*n},(_,k)=>a.componentType===5126?b.readFloatLE(offset+k*bytes):b.readUInt16LE(offset+k*bytes));}
 for(let i=0;i<d.accessors.length;i++)assert(read(i).every(Number.isFinite),'Nonfinite vertex');
 for(const m of d.meshes)for(const p of m.primitives){const count=d.accessors[p.attributes.POSITION].count,ix=read(p.indices);assert(ix.length%3===0);assert(ix.every(i=>i<count),'Index outside vertex buffer');assert.equal(count,d.accessors[p.attributes.NORMAL].count);assert.equal(count,d.accessors[p.attributes.TEXCOORD_0].count);assert(p.material<d.materials.length);triangles+=ix.length/3;
   const pos=read(p.attributes.POSITION),normal=read(p.attributes.NORMAL);
   for(let k=0;k<ix.length;k+=3){const a=ix[k]*3,c=ix[k+1]*3,f=ix[k+2]*3,u=pos.slice(c,c+3).map((v,j)=>v-pos[a+j]),v=pos.slice(f,f+3).map((v,j)=>v-pos[a+j]);const cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]];assert(cross.reduce((s,v,j)=>s+v*normal[a+j],0)>=-1e-5,'Incorrect winding');}
 }
 for(const m of d.materials){assert(m.emissiveFactor==null||m.emissiveFactor.every(v=>v>=0&&v<=1));assert(m.pbrMetallicRoughness.roughnessFactor>=0);}
 if(['ssao','ssr','ssgi'].includes(e.id)){assert(on[e.id].enabled);assert(!off[e.id].enabled);}
 if(e.id==='taa'){assert(on.taa.enabled);assert(!off.taa.enabled);}
}
assert.equal(catalog.length,32);
const actor=JSON.parse(fs.readFileSync(path.join(root,'data/showcases/actor.gltf')));
assert.equal(actor.animations[0].channels.length,2);
assert.deepEqual(actor.animations[0].channels.map(c=>c.target.path),['translation','rotation']);
assert.equal(actor.accessors[actor.animations[0].samplers[0].input].count,5);
console.log(`Validated ${catalog.length} exhibits, ${triangles} triangles, vertex/index ranges, winding and comparison profiles.`);
