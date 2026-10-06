// Recompose actual Figma SVG layers. Reference screenshots are never inputs.
const fs=require('node:fs'),path=require('node:path'),{createRequire}=require('node:module');
const runtime=process.env.CODEX_NODE_MODULES||path.join(process.env.USERPROFILE,'.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules');
const sharp=createRequire(path.join(runtime,'_resolver.cjs'))('sharp');
const root=path.resolve(__dirname,'..'),out=path.join(root,'assets/processed/summary');
function source(id,name){return fs.readFileSync(path.join(root,'assets/source/summary',id,name+'.svg'),'utf8');}
function image(svg,x,y,w,h){return '<image href="data:image/svg+xml;base64,'+Buffer.from(svg).toString('base64')+'" x="'+x+'" y="'+y+'" width="'+w+'" height="'+h+'"/>';}
async function canvas(name,parts){
 const svg='<svg xmlns="http://www.w3.org/2000/svg" width="240" height="320">'+parts.join('')+'</svg>';
 const big=await sharp(Buffer.from(svg),{density:288}).png().toBuffer();
 await sharp(big).resize(240,320).png().toFile(path.join(out,name+'.png'));
}
(async()=>{
 fs.mkdirSync(out,{recursive:true});
 const themes=JSON.parse(fs.readFileSync(path.join(root,'design/character-registry.json'),'utf8')).characters;
 for(const t of themes){
  for(const [kind,id,y,h] of [['intro','102-2398',54,47],['end','102-3214',38,74]]){
   const svg=source(id,'imgUnion').replaceAll('#F6E2FF','#'+t.panel);
   await canvas(kind+'-'+t.id,[image(svg,8,y,226,h)]);
  }
 }
 await canvas('closed',[image(source('102-3092','imgGroup325'),16,110,207,125)]);
 await canvas('back-short',[image(source('102-3099','imgUnion'),9.1,87,226.9,171.35)]);
 await canvas('back',[image(source('102-3110','imgUnion'),9.1,53,226.9,214.25)]);
 await canvas('front',[image(source('102-3099','imgSubtract'),8,121.65,226.9,136.7)]);
 for(const [row,x,y,w,h] of [[0,170.45,154.7,20.74,26.394],[1,165.87,183.3,28.502,30.16],[2,168.44,222.24,29.526,24.233]])
  await canvas('check-'+row,[image(source('102-3170','imgVector'+(43+row)),x,y,w,h)]);
 const arrow=source('102-3170','imgPolygon22');
 // The SVG is already tightly cropped to the triangle (29.4449 x 28.5).
 await canvas('arrows',['<g transform="translate(212.5 150.27755) rotate(90 14.25 14.72245)">'+image(arrow,-.47245,.47245,29.4449,28.5)+'</g>',
  '<g transform="translate(3 150.27755) rotate(-90 14.25 14.72245)">'+image(arrow,-.47245,.47245,29.4449,28.5)+'</g>']);
 const stress=[[38.98,33.67,3.096,41.633],[47.21,40.83,1.3,24.059],[57.13,32.01,8.611,56.267],[70.16,39.38,5.487,31.072],[78.82,41.3,3.843,26.6],[94.31,47.94,3.144,14.015],[106.36,46.08,3.915,16.744],[116.76,38.71,3.696,23.624],[127.71,36.37,2.445,29.891],[135.08,39.4,4.571,29.751],[146.46,47.18,5.003,35.172],[157.12,42.16,6.341,57.897],[167.25,52.2,4.374,22.852],[177.43,46.08,1.293,30.857],[189.57,53.32,0,11.471],[193.44,46.3,4.262,19.023]];
 await canvas('unfinished',stress.map(([x,y,w,h],i)=>image(source('102-3191','imgVector'+(47+i)),x-1,y-1,w+2,h+2)));
 console.log('Prepared summary bubbles, letter layers, outcome marks and arrows from Figma SVGs');
})().catch(e=>{console.error(e);process.exitCode=1;});
