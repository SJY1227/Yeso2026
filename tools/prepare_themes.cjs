// Compose retained Figma source layers; QA screenshots are never inputs.
const fs=require('node:fs'),path=require('node:path'),{createRequire}=require('node:module');
const runtime=process.env.CODEX_NODE_MODULES||path.join(process.env.USERPROFILE,'.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules');
const sharp=createRequire(path.join(runtime,'_resolver.cjs'))('sharp');
const root=path.resolve(__dirname,'..'),out=path.join(root,'assets/processed/themes');
const manifest=JSON.parse(fs.readFileSync(path.join(root,'design/theme-assets.json'),'utf8'));
const file=(id,name)=>path.join(root,'assets/source/themes',manifest.frames.find(f=>f.id===id).assets.find(a=>!name||a.name===name).file);
const ctx=id=>fs.readFileSync(path.join(root,'design/context/theme-'+id.replace(':','-')+'.txt'),'utf8');
const records=[
 {key:'sheep',name:'몽실이',home:'104:894',stages:['554:2487','554:2488','554:2442'],panel:'f6e2ff',button:'7f6889',accent:'d591f2',track:'ffffff'},
 {key:'red-robot',name:'또비',home:'321:1777',stages:['674:2631','674:2634','554:2441'],panel:'f2edda',button:'6a6a6a',accent:'0ed9f7',track:'ffffff'},
 {key:'horned',name:'메에',home:'425:8379',locked:'425:8409',stages:['554:2485','554:2486','554:2445'],panel:'d8e4a9',button:'7c8e24',accent:'c1ec00',bg:['imgImage1',0,-13.984,282,361.984],char:['imgImage138',43,94,140,187,4.28]},
 {key:'green-robot',name:'저스티스',home:'509:2630',locked:'509:2660',stages:['677:2637','677:2640','554:2443'],panel:'c2eeba',button:'218c3a',accent:'0cf966',bg:['imgImage198',0,0,240,320],char:['imgImage175',46,94,147,188]},
 {key:'cat',name:'냐요미',home:'483:4292',locked:'483:4322',stages:['554:2483','554:2484','554:2455'],panel:'f0e0d9',button:'ae8901',accent:'fbff00',bg:['imgImage160',-40.056,7,302.112,322.0144],char:['imgImage140',33,92,174,192]},
 {key:'dog',name:'맘뭉이',home:'509:2948',locked:'509:2979',stages:['554:2481','554:2482','554:2444'],panel:'ffee8d',button:'c16a00',accent:'fbff00',bg:['imgImage194',-144.2448,0,429.7242,322],char:['imgImage178',50,92,147,192]},
 {key:'seal',name:'물범',namePending:true,home:'483:4646',locked:'483:4675',stages:['554:2453','554:2454','554:2449'],panel:'cef7ff',button:'009ac9',accent:'b0efff',bg:['imgImage110',-98.2096,-.16,399.7944,320.128],char:['imgImage168',45,116,144,175]},
 {key:'blue-robot',name:'파랑 로봇',namePending:true,home:'640:2633',locked:'671:2584',stages:['677:2643','677:2646','554:2446'],panel:'76ecdb',button:'009389',accent:'1cffd9',bg:['imgImage328',-7,-5,262,350],char:['imgImage329',60,94,130,195]},
 {key:'bear',name:'북극곰',home:'640:2382',locked:'640:2586',stages:['554:2451','554:2452','554:2450'],panel:'a2bad7',button:'33b7ff',accent:'c9e1f7',bg:['imgImage267',0,0,252,336],char:['imgImage268',56,98,126.744,201]},
 {key:'rabbit',name:'깡충이',home:'640:2796',locked:'671:2397',stages:['554:2456','554:2457','554:2447'],panel:'ffc8c7',button:'f272d0',accent:'f272d0',bg:['imgImage307',-9,0,265,354],char:['imgImage308',38,103,167,193]}
];
function dataUri(filename){return 'data:'+(filename.endsWith('.svg')?'image/svg+xml':'image/png')+';base64,'+fs.readFileSync(filename).toString('base64');}
function img(id,name,x,y,w,h,angle=0,fit='none'){
 const href=dataUri(file(id,name));
 if(!angle)return `<image href="${href}" x="${x}" y="${y}" width="${w}" height="${h}" preserveAspectRatio="${fit}"/>`;
 const a=angle*Math.PI/180,bw=Math.abs(w*Math.cos(a))+Math.abs(h*Math.sin(a)),bh=Math.abs(w*Math.sin(a))+Math.abs(h*Math.cos(a));
 return `<g transform="translate(${x+bw/2} ${y+bh/2}) rotate(${angle})"><image href="${href}" x="${-w/2}" y="${-h/2}" width="${w}" height="${h}" preserveAspectRatio="${fit}"/></g>`;
}
async function canvas(parts,stem,white=false){
 const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="240" height="320">${white?'<rect width="240" height="320" fill="white"/>':''}${parts.join('')}</svg>`;
 const big=await sharp(Buffer.from(svg),{density:288}).png().toBuffer();
 await sharp(big).resize(240,320).png().toFile(path.join(out,stem+'.png'));
}
function lockedSlot(id){
 const lines=ctx(id).split('\n'),i=lines.findIndex(l=>l.includes('data-name="image'));
 const c=lines[i].match(/className="([^"]+)"/)[1],dim=(s,k)=>Number(s.match(new RegExp(k+'-\\[([\\d.-]+)px\\]'))?.[1]);
 const w=dim(c,'w'),h=dim(c,'h'),rot=lines[i-1].match(/rotate-\[([\d.]+)deg\]/);
 const parent=rot?lines[i-2].match(/className="([^"]+)"/)[1]:c;
 let x=dim(parent,'left');const calc=parent.match(/left-\[calc\(50%([+-])([\d.]+)px\)\]/);
 if(calc)x=120+(calc[1]==='-'?-1:1)*Number(calc[2]);
 if(parent.includes('left-1/2'))x=120;
 if(parent.includes('-translate-x-1/2'))x-=dim(parent,'w')/2;
 const name=lines.slice(i,i+6).join('\n').match(/src=\{(\w+)\}/)[1];
 return [name,x,dim(parent,'top'),w,h,rot?Number(rot[1]):0];
}
(async()=>{
 fs.mkdirSync(out,{recursive:true});
 const oldHomes=JSON.parse(fs.readFileSync(path.join(root,'design/home-assets.json'),'utf8')).themes;
 for(let i=0;i<records.length;i++){
   const r=records[i];r.id=i;r.track||='454545';
   let layers=[];
   if(i<2){const old=oldHomes[i];r.bg=[old.background.name,0,0,240,320];r.char=[old.character.name,old.character.x,old.character.y,old.character.w,old.character.h];}
   layers.push(img(r.home,...r.bg));
   if(i===0)layers.push('<defs><linearGradient id="g" x2="0" y2="1"><stop stop-color="black" stop-opacity="0"/><stop offset="1" stop-color="black" stop-opacity=".61"/></linearGradient></defs><rect x="0" y="200" width="240" height="120" fill="url(#g)"/>');
   if(i===6)layers.push('<defs><linearGradient id="g" x2="0" y2="1"><stop stop-color="#009ac9" stop-opacity="0"/><stop offset="1" stop-color="#121212"/></linearGradient></defs><rect x="-5" y="249" width="245" height="100" fill="url(#g)"/>');
   const top=img(r.home,'imgRectangle34629287',0,0,240,27),icons=img(r.home,'imgGroup543',0,0,240,27);
   await canvas([...layers,top,icons],'background-'+i,true);
   // Keep the speech bubble separate so motion never duplicates the background.
   await canvas([img(r.home,'imgUnion',16,43,207,61)],'homebase-'+i);
   layers.push(top,img(r.home,'imgUnion',16,43,207,61),icons,img(r.home,...r.char));
   await canvas(layers,'home-'+i,true);await canvas([top,icons],'topbar-'+i);
   const oldCatalog=JSON.parse(fs.readFileSync(path.join(root,'design/catalog-assets.json'),'utf8'));
   const arrowFile=i<2?path.join(root,'assets/source/catalog',oldCatalog.frames.find(f=>f.id===(i?'321:3569':'106:2527')).assets.find(a=>a.name==='imgPolygon17').file):file(r.locked,'imgPolygon17');
   const arrow=await sharp(arrowFile,{density:288}).png().toBuffer();
   const arrows=await sharp({create:{width:960,height:1280,channels:4,background:'#00000000'}}).composite([
    {input:await sharp(arrow).rotate(270).png().toBuffer(),left:4,top:Math.round(195.27755*4)},
    {input:await sharp(arrow).rotate(90).png().toBuffer(),left:Math.round(209.5*4),top:Math.round(195.27755*4)}
   ]).png().toBuffer();
   await sharp(arrows).resize(240,320).png().toFile(path.join(out,'arrows-'+i+'.png'));
   for(let stage=0;stage<3;stage++){
     // Home references show representative stages, not a complete placement set.
     // Use each actual source stage at a common baseline, keeping its aspect ratio.
     // These slot sizes are adjustable layout policy, recorded for designer review.
     const stageId=r.stages[stage],meta=await sharp(file(stageId)).metadata();
     const scale=Math.min(180/meta.width,[150,175,187][stage]/meta.height);
     const sw=meta.width*scale,sh=meta.height*scale;
     await canvas([img(stageId,null,120-sw/2,278-sh,sw,sh)],'homecharacter-'+(i*3+stage));
     if(i<2){fs.copyFileSync(path.join(root,`assets/processed/catalog/characters-${i*3+stage}.png`),path.join(out,`character-${i*3+stage}.png`));continue;}
     const id=r.stages[stage];const s=ctx(id);
     const w=Number(s.match(/width="([\d.]+)"/)?.[1]||meta.width),h=Number(s.match(/height="([\d.]+)"/)?.[1]||meta.height);
     // New stages have source art but no individual catalog placement. Keep aspect
     // ratio in the shared slot; growth sizes (small/medium/full) await design signoff.
     const k=Math.min(166/w,(stage===0?125:stage===1?150:170)/h),cw=w*k,ch=h*k;
     await canvas([img(id,null,120-cw/2,258-ch,cw,ch)],'character-'+(i*3+stage));
   }
   if(r.locked){r.lockedSlot=lockedSlot(r.locked);await canvas([img(r.locked,...r.lockedSlot)],'locked-'+(i*3+2));}
 }
 fs.writeFileSync(path.join(root,'design/character-registry.json'),JSON.stringify({schema:1,source:'Figma + user 2026-10-04: ten species, three stages',characters:records},null,2)+'\n');
 console.log('Prepared 10 homes/top bars, 30 stages and 8 additional source silhouettes');
})().catch(e=>{console.error(e);process.exitCode=1;});
