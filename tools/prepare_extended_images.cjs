// Additional individual Figma layers, retaining source shape/color/alpha.
const fs=require('node:fs'),path=require('node:path'),{createRequire}=require('node:module');
const runtime=process.env.CODEX_NODE_MODULES||path.join(process.env.USERPROFILE,'.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules');
const sharp=createRequire(path.join(runtime,'_resolver.cjs'))('sharp');
const root=path.resolve(__dirname,'..'),out=path.join(root,'assets/processed/themes');
const ext=JSON.parse(fs.readFileSync(path.join(root,'design/extended-assets.json'),'utf8'));
const old=JSON.parse(fs.readFileSync(path.join(root,'design/routine-assets.json'),'utf8'));
const catalog=JSON.parse(fs.readFileSync(path.join(root,'design/catalog-assets.json'),'utf8'));
function file(manifest,folder,id,name){return path.join(root,'assets/source',folder,manifest.frames.find(f=>f.id===id).assets.find(a=>a.name===name).file);}
function image(filename,x,y,w,h){return `<image href="data:${filename.endsWith('.svg')?'image/svg+xml':'image/png'};base64,${fs.readFileSync(filename).toString('base64')}" x="${x}" y="${y}" width="${w}" height="${h}" preserveAspectRatio="none"/>`;}
async function canvas(parts,stem){
 const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="240" height="320">${parts.join('')}</svg>`;
 const hi=await sharp(Buffer.from(svg),{density:288}).png().toBuffer();
 await sharp(hi).resize(240,320).png().toFile(path.join(out,stem+'.png'));
}
(async()=>{
 const ids=['105:1231','321:3371','425:8458','509:2693','483:4355','509:3011','483:4707','640:2698','640:2418','640:2816'];
 const paperColors=[],buttonColors=[];
 for(let i=0;i<ids.length;i++){
   const id=ids[i],first=i===0;
   const src=name=>file(first?old:ext,first?'routine':'extended',id,name);
   const ctx=first?'':fs.readFileSync(path.join(root,'design/context/extended-'+id.replace(':','-')+'.txt'),'utf8');
   const color=first?'f6e2ff':ctx.match(/bg-\[#([0-9a-f]+)\] h-\[251px\]/)[1];
   const button=first?'6b6969':ctx.match(/bg-\[#([0-9a-f]+)\] h-\[39px\] left-\[25px\]/)[1];
   paperColors.push(color);buttonColors.push(button);
   const bottomX=i===5?8:9;
   await canvas([image(src('imgUnion'),9.1,137,226.9,214.25),`<rect x="20" y="52" width="202" height="251" fill="#${color}"/>`,image(src('imgSubtract'),bottomX,214.55,226.9,136.7)],'paper-'+i);
 }
 // Food identity uses all 31 original sources; reuse catalog source mapping,
 // preserving aspect ratio in the reward slot (blueberry reference 73,181,103,79).
 const geometry=JSON.parse(fs.readFileSync(path.join(root,'design/catalog-geometry.json'),'utf8'));
 for(let i=0;i<31;i++){
   const g=geometry.foods[i],f=file(catalog,'catalog',g.id,g.src),m=await sharp(f).metadata();
   const k=Math.min(103/m.width,79/m.height),w=m.width*k,h=m.height*k;
   await canvas([image(f,124.5-w/2,258-h,w,h)],'rewardfood-'+i);
 }
 // The existing envelope is a standalone vector, not a screenshot of the screen.
 await canvas([image(file(old,'routine','104:926','imgUnion'),156,51,72,74),'<rect x="170" y="73" width="44" height="26" fill="white"/>',image(file(old,'routine','104:926','imgEllipse171'),210,65,16,16)],'letter-notification');
 fs.writeFileSync(path.join(root,'design/extended-geometry.json'),JSON.stringify({schema:1,routineFrames:ids,paperColors,buttonColors,stageHomeSlot:{centerX:120,baseline:278,maxWidth:180,heights:[150,175,187],placement:'provisional, source aspect ratio retained'},rewardSlot:{centerX:124.5,bottom:258,maxWidth:103,maxHeight:79}},null,2)+'\n');
 console.log('Prepared ten paper themes, 31 reward foods, and reusable envelope');
})().catch(e=>{console.error(e);process.exitCode=1;});
