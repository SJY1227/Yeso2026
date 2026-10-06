// Preserve each downloaded Figma image/SVG and its source design slot.
const fs=require('node:fs'),path=require('node:path'),{createRequire}=require('node:module');
const runtime=process.env.CODEX_NODE_MODULES||path.join(process.env.USERPROFILE,'.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules');
const sharp=createRequire(path.join(runtime,'_resolver.cjs'))('sharp');
const root=path.resolve(__dirname,'..'),manifest=JSON.parse(fs.readFileSync(path.join(root,'design/catalog-assets.json'),'utf8'));
const out=path.join(root,'assets/processed/catalog'),scale=4;
const source=(id,name)=>path.join(root,'assets/source/catalog',manifest.frames.find(f=>f.id===id).assets.find(a=>a.name===name).file);
const dimension=(classes,key)=>Number(classes.match(new RegExp(key+'-\\[([\\d.]+)px\\]'))?.[1]);
function left(classes,width){
  let x=dimension(classes,'left');
  const calc=classes.match(/left-\[calc\(50%([+-])([\d.]+)px\)\]/);
  if(calc)x=120+(calc[1]==='-'?-1:1)*Number(calc[2]);
  if(classes.includes('left-1/2'))x=120;
  if(classes.includes('-translate-x-1/2'))x-=width/2;
  return x;
}
function slot(id){
  const context=fs.readFileSync(path.join(root,'design/context/catalog-'+id.replace(':','-')+'.txt'),'utf8');
  const lines=context.split('\n');
  const index=lines.findIndex(l=>l.includes('data-name=')&&!l.includes('16. 도감'));
  if(index<0)throw Error('Missing artwork '+id);
  const classes=lines[index].match(/className="([^"]+)"/)[1];
  const w=dimension(classes,'w'),h=dimension(classes,'h');
  const src=lines.slice(index,index+6).join('\n').match(/src=\{(\w+)\}/)[1];
  const rotation=lines[index-1].match(/rotate-\[([\d.]+)deg\]/);
  let result={id,src,x:left(classes,w),y:dimension(classes,'top'),w,h,rotation:0};
  if(rotation){
    const parent=lines[index-2].match(/className="([^"]+)"/)[1];
    result={...result,x:left(parent,dimension(parent,'w')),y:dimension(parent,'top'),rotation:Number(rotation[1])};
  }
  if(id==='321:2336')result.crop={x:-.096*w,y:-.1934*h,w:1.1928*w,h:1.1938*h};
  const paragraph=context.match(/<p className="([^"]*text-\[20px\][^"]*)"[^>]*>\s*([^<>]+)\s*<\/p>/g)||[];
  const name=paragraph.find(p=>!p.includes('text-white'));
  const nameClass=name?.match(/className="([^"]+)"/)?.[1]||'';
  result.nameX=left(nameClass,0);result.nameTop=dimension(nameClass,'top');
  result.number=Number(context.match(/\n\s+(\d+)\/31\s*\n/)?.[1]||0);
  if(![result.x,result.y,w,h].every(Number.isFinite))throw Error('Invalid slot '+JSON.stringify(result));
  return result;
}
async function artwork(s){
  const image=source(s.id,s.src);
  let data;
  if(s.crop){
    const inner=s.crop;
    const resized=await sharp(image).resize(Math.round(inner.w*scale),Math.round(inner.h*scale),{fit:'fill'}).png().toBuffer();
    data=await sharp(resized).extract({left:Math.round(-inner.x*scale),top:Math.round(-inner.y*scale),width:Math.round(s.w*scale),height:Math.round(s.h*scale)}).png().toBuffer();
  } else data=await sharp(image).resize(Math.round(s.w*scale),Math.round(s.h*scale),{fit:'cover'}).png().toBuffer();
  if(s.rotation)data=await sharp(data).rotate(s.rotation,{background:'#00000000'}).png().toBuffer();
  return {input:data,left:Math.round(s.x*scale),top:Math.round(s.y*scale)};
}
async function canvas(layers,name){
  // All catalogue artwork fits within the frame at its retained design slot.
  const composed=await sharp({create:{width:240*scale,height:320*scale,channels:4,background:'#00000000'}})
    .composite(layers).png().toBuffer();
  await sharp(composed).resize(240,320).png().toFile(path.join(out,name+'.png'));
}
(async()=>{
  fs.mkdirSync(out,{recursive:true});
  const characters=['106:2527','106:2601','106:2574','321:3569','321:2364','321:2336'];
  const locked=['321:2301','321:2280','321:2262','106:2653','106:2632','106:2506'];
  const geometry={characters:[],locked:[],foods:[]};
  for(const [category,ids] of [['characters',characters],['locked',locked]]){
    for(let i=0;i<ids.length;i++){
      const s=slot(ids[i]);geometry[category].push(s);
      await canvas([await artwork(s)],category+'-'+i);
    }
  }
  for(const frame of manifest.frames.filter(f=>f.id.startsWith('466:'))){
    const s=slot(frame.id);geometry.foods[s.number-1]=s;
    await canvas([await artwork(s)],'food-'+s.number);
  }
  for(const [name,id] of [['topbar-pink','106:2527'],['topbar-dark','321:3569']])
    await canvas([{input:await sharp(source(id,'imgGroup545'),{density:72*scale}).png().toBuffer(),left:0,top:0}],name);
  const arrow=await sharp(source('106:2527','imgPolygon17'),{density:72*scale}).png().toBuffer();
  await canvas([
    {input:await sharp(arrow).rotate(270).png().toBuffer(),left:4,top:Math.round(195.27755*scale)},
    {input:await sharp(arrow).rotate(90).png().toBuffer(),left:Math.round(209.5*scale),top:Math.round(195.27755*scale)}
  ],'arrows');
  fs.writeFileSync(path.join(root,'design/catalog-geometry.json'),JSON.stringify(geometry,null,2)+'\n');
  console.log('Prepared 6 character stages, 6 source silhouettes, 31 foods, original arrows and top bars');
})().catch(e=>{console.error(e);process.exitCode=1;});
