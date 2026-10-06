// Composite individual original assets; reference frame screenshots are not inputs.
const fs=require('node:fs'), path=require('node:path'), {createRequire}=require('node:module');
const runtime=process.env.CODEX_NODE_MODULES||path.join(process.env.USERPROFILE,'.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules');
const sharp=createRequire(path.join(runtime,'_resolver.cjs'))('sharp');
const root=path.resolve(__dirname,'..'), out=path.join(root,'assets/processed');
const manifest=JSON.parse(fs.readFileSync(path.join(root,'design/routine-assets.json'),'utf8'));
const file=(id,name)=>path.join(root,'assets/source/routine',manifest.frames.find(f=>f.id===id).assets.find(a=>a.name===name).file);
const layer=async(id,name,x,y,w,h,fit='fill')=>({input:await sharp(file(id,name)).resize(w,h,{fit}).png().toBuffer(),left:x,top:y});
const rect=(x,y,w,h,color)=>({input:{create:{width:w,height:h,channels:4,background:color}},left:x,top:y});
async function flat(layers,name,transparent=false) {
  // Clip oversized Figma slots without changing their original geometry.
  const clipped=[];
  for(const l of layers){
    const data=await sharp(l.input).png().toBuffer(), meta=await sharp(data).metadata();
    const x=Math.max(0,l.left),y=Math.max(0,l.top),right=Math.min(240,l.left+meta.width),bottom=Math.min(320,l.top+meta.height);
    if(right>x&&bottom>y)clipped.push({input:await sharp(data).extract({left:x-l.left,top:y-l.top,width:right-x,height:bottom-y}).toBuffer(),left:x,top:y});
  }
  await sharp({create:{width:240,height:320,channels:4,background:transparent?'#00000000':'#ffffff'}}).composite(clipped).png().toFile(path.join(out,name+'.png'));
}
async function background(id){
  const g=Buffer.alloc(240*120*4); for(let y=0;y<120;y++)for(let x=0;x<240;x++)g[(y*240+x)*4+3]=Math.round(255*.61*y/119);
  return [await layer(id,'imgImage78',-44,-22,383,383),{input:await sharp(g,{raw:{width:240,height:120,channels:4}}).png().toBuffer(),left:0,top:200},await layer(id,'imgGroup545',0,0,240,27)];
}
(async()=>{
  fs.mkdirSync(out,{recursive:true});
  let id='104:926',layers=await background(id);
  layers.push(await layer(id,'imgImage68',24,58,177,237,'cover'),await layer(id,'imgUnion',156,51,72,74),rect(170,73,44,26,'#ffffff'),await layer(id,'imgEllipse171',210,65,16,16));
  await flat(layers,'product-letter');
  id='105:1231';layers=await background(id);
  layers.push(await layer(id,'imgUnion',9,137,227,214),rect(20,52,202,251,'#f6e2ff'),await layer(id,'imgSubtract',8,215,227,137));
  await flat(layers,'product-routine');
  id='106:2196';layers=[await layer(id,'imgImage68',38,68,163,219,'cover'),await layer(id,'imgUnion',16,43,207,61),await layer(id,'imgGroup545',0,0,240,27)];
  await flat(layers,'product-catalog');
  id='102:2470';layers=[await layer(id,'imgImage68',38,53,163,219,'cover'),await layer(id,'imgGroup546',120,169,21,26),await layer(id,'imgImage245',112,148,44,65)];
  await flat(layers,'product-cry',true);
  id='105:1391';
  layers=[await layer(id,'img1',73,181,103,79)];
  await flat(layers,'product-blueberry',true);
  console.log('Prepared product backgrounds, crying character, and original blueberry artwork');
})().catch(e=>{console.error(e);process.exitCode=1;});
