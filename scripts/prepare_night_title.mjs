// Palette-only night variants of the ORIGINAL art; no generated scenery joins.
// Run with Node. The sky itself is generated at runtime.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const title=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../tracker/packaging/common/title');
function read(name){
 const b=fs.readFileSync(path.join(title,name));
 const w=b.readInt32LE(18),signedH=b.readInt32LE(22),h=Math.abs(signedH),bits=b.readUInt16LE(28),start=b.readUInt32LE(10);
 if(b.toString('ascii',0,2)!=='BM'||![24,32].includes(bits)||b.readUInt32LE(30)!==0)throw Error('Unsupported BMP '+name);
 const step=bits/8,stride=(w*step+3)&~3,p=new Uint8ClampedArray(w*h*4);
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){
  const s=start+(signedH<0?y:h-1-y)*stride+x*step,i=(y*w+x)*4;
  const r=b[s+2],g=b[s+1],blue=b[s];
  p.set([r,g,blue,r>100&&blue>100&&r>g*1.6&&blue>g*1.6?0:255],i);
 }
 return {w,h,p};
}
function write(name,{w,h,p}){
 const stride=(w*3+3)&~3,b=Buffer.alloc(54+stride*h);
 b.write('BM');b.writeUInt32LE(b.length,2);b.writeUInt32LE(54,10);b.writeUInt32LE(40,14);
 b.writeInt32LE(w,18);b.writeInt32LE(h,22);b.writeUInt16LE(1,26);b.writeUInt16LE(24,28);b.writeUInt32LE(stride*h,34);
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){
  const i=(y*w+x)*4,o=54+(h-1-y)*stride+x*3;
  b[o]=p[i+3]?p[i+2]:255;b[o+1]=p[i+3]?p[i+1]:0;b[o+2]=p[i+3]?p[i]:255;
 }
 fs.writeFileSync(path.join(title,name),b);
}
for(const kind of ['sky','scene','foreground','viaduct','train','logo']){
 const image=read(`snes_${kind}.bmp`),{w,h,p}=image;
 if(kind==='sky')for(let x=0;x<w;x++){
  let top=h;
  for(let y=90;y<h-2;y++){
   let solid=true;for(let d=0;d<3;d++){const i=((y+d)*w+x)*4;if(p[i]<60||p[i+1]<70){solid=false;break;}}
   if(solid){top=y;break;}
  }
  for(let y=0;y<top;y++)p[(y*w+x)*4+3]=0;
 }
 for(let i=0;i<p.length;i+=4){
  if(!p[i+3])continue;
  const r=p[i],g=p[i+1],b=p[i+2],y=.2126*r+.7152*g+.0722*b;
  if(kind==='logo'){
   const v=60+Math.sqrt(y/255)*116;p[i]=Math.round(v*.78);p[i+1]=Math.round(v*.88);p[i+2]=Math.round(v);
  }else{
   const v=(kind==='sky'?20:14)+Math.pow(y/255,.68)*(kind==='sky'?35:65);
   p[i]=Math.round(v*.65+(r-y)*.10);p[i+1]=Math.round(v*.82+(g-y)*.09);p[i+2]=Math.round(v*1.14+(b-y)*.07);
  }
 }
 write(`night_${kind==='sky'?'mountains':kind}.bmp`,image);
}
const colors=['18253b','24374f','354c68','49627e','6c88a5','9fb6cc','d0dce7','e5eaf0'];
write('night_meteor.bmp',{w:8,h:1,p:new Uint8ClampedArray(colors.flatMap(c=>[parseInt(c.slice(0,2),16),parseInt(c.slice(2,4),16),parseInt(c.slice(4,6),16),255]))});
console.log('Night title variants generated from the original assets.');
