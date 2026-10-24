const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
require('node:fs').mkdirSync(path.join(root,'.validation'),{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--disable-gpu'],executablePath:process.env.CHROME_EXECUTABLE || undefined});
 try{
 const page=await browser.newPage({viewport:{width:1440,height:1000}});const errors=[],requests=[];
 page.on('pageerror',e=>errors.push(e.message));page.on('request',r=>{if(r.url().startsWith('http'))requests.push(r.url())});
 await page.goto('file://'+root+'/Choo-Choo-User-Guide.html');
 await page.evaluate(()=>{document.documentElement.style.scrollBehavior='auto';return document.fonts.ready});
 assert.equal(await page.locator('.page').count(),26);assert.equal(await page.locator('#cloning').count(),1);
 assert.equal(await page.locator('#start [data-example],#cloning [data-example]').count(),0);
 const buttons=page.locator('[data-example]');const count=await buttons.count();assert.equal(count,155);
 for(const id of ['song','chain','phrase','table','groove']){
  for(const name of ['SELECT + OPT','OPT, with a selection','OPT + EDIT, with a selection']){
   assert.equal(await page.locator('#'+id+' .control-example').filter({hasText:name}).count()>0,true);
  }
 }
 let opened=0;
 for(let i=0;i<count;i++){
  const button=buttons.nth(i);const id=await button.getAttribute('data-example');
  await button.evaluate(el=>el.click());
  await page.locator('#example-image').evaluate(img=>img.decode());
  assert.equal(await page.locator('#example-view').evaluate(el=>el.open),true,id);
  assert.equal(await page.locator('#example-image').evaluate(el=>el.naturalWidth),640,id);
  assert.equal(await page.locator('#example-view button').count(),1,'Only Close, no play controls');
  await page.keyboard.press('Escape');
  await page.waitForFunction(id=>document.activeElement?.dataset.example===id,id);
  assert.equal(await button.evaluate(el=>document.activeElement===el),true,'Focus restored '+id);
  assert.equal(await page.locator('#example-image').getAttribute('src'),null,'GIF unloaded');
  opened++;
 }
 for(const [name,width,height] of [['desktop',1440,1000],['phone',390,844]]){
  await page.setViewportSize({width,height});
  const trigger=page.locator('#song [data-example="song-02"]');
  await trigger.evaluate(el=>el.scrollIntoView({block:'center',behavior:'instant'}));
  await trigger.hover();
  assert.equal(await trigger.evaluate(el=>getComputedStyle(el).color),'rgb(178, 34, 34)');
  assert.equal(await trigger.locator('[role=tooltip]').textContent(),'Click to view example');
  assert.equal(await trigger.locator('[role=tooltip]').evaluate(el=>getComputedStyle(el).visibility),'visible');
  await page.screenshot({path:root+'/.validation/examples-'+name+'-hover.png'});
  const before=await page.evaluate(()=>scrollY);await trigger.click();
  await page.locator('#example-image').evaluate(img=>img.decode());
  const bounds=await page.locator('#example-view').boundingBox();assert.ok(bounds.x>=0&&bounds.x+bounds.width<=width+1);
  await page.screenshot({path:root+'/.validation/examples-'+name+'-popup.png'});
  await page.locator('#example-view .close-view').click();
  assert.ok(Math.abs(await page.evaluate(()=>scrollY)-before)<2,'Scroll position retained');
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  // Keyboard activation and backdrop dismissal.
  await trigger.focus();await page.keyboard.press('Enter');
  assert.equal(await page.locator('#example-view').evaluate(el=>el.open),true);
  await page.mouse.click(2,2);assert.equal(await page.locator('#example-view').evaluate(el=>el.open),false);
  await page.locator('#cloning').evaluate(el=>el.scrollIntoView({block:'start',behavior:'instant'}));
  await page.waitForTimeout(80);assert.equal(await page.locator('#contents a[href="#cloning"]').getAttribute('aria-current'),'location');await page.screenshot({path:root+'/.validation/examples-'+name+'-cloning.png'});
 }
 assert.deepEqual(errors,[]);assert.deepEqual(requests,[]);
 // The smaller editable edition also resolves its GIF assets locally.
 await page.goto('file://'+root+'/index.html');await page.locator('#song [data-example="song-02"]').evaluate(el=>el.click());
 await page.locator('#example-image').evaluate(img=>img.decode());
 console.log(JSON.stringify({opened,standaloneOffline:true,editableAssets:true,desktopAndPhone:true,hoverRed:true,tooltip:true,keyboard:true,backdrop:true,focusRestored:true,scrollRetained:true,errors},null,2));
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
