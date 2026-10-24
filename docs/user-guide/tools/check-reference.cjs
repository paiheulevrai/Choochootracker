const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
require('node:fs').mkdirSync(path.join(root,'.validation'),{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--disable-gpu'],executablePath:process.env.CHROME_EXECUTABLE || undefined});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:1000}});
  const errors=[],requests=[];page.on('pageerror',e=>errors.push(e.message));page.on('request',r=>{if(/^https?:/.test(r.url()))requests.push(r.url())});
  await page.goto('file://'+root+'/Choo-Choo-User-Guide.html');
  await page.evaluate(()=>{document.documentElement.style.scrollBehavior='auto';return document.fonts.ready});
  assert.equal(await page.locator('.page').count(),26);
  assert.equal(await page.locator('.reference-topic').count(),92);
  assert.equal(await page.locator('.reference-topic[open]').count(),0);
  assert.deepEqual(await page.locator('.nav-group>span').allTextContents(),['Arrange','Sound','Mix','Project','Settings']);
  assert.equal(await page.locator('#guide-font option').count(),8);
  assert.equal(await page.locator('body').evaluate(el=>el.innerText.includes('Auto Mix')),false);
  const integrity=await page.evaluate(()=>{
    const ids=[...document.querySelectorAll('[id]')].map(el=>el.id);
    return {duplicates:ids.filter((id,i)=>ids.indexOf(id)!==i),broken:[...document.querySelectorAll('a[href^="#"]')].map(el=>el.hash.slice(1)).filter(id=>!document.getElementById(id))};
  });assert.deepEqual(integrity,{duplicates:[],broken:[]});
  // Every reference can be reached directly; closed parents open on navigation.
  for(const id of await page.locator('.reference-topic').evaluateAll(els=>els.map(el=>el.id))){
    await page.evaluate(id=>{location.hash=id},id);
    await page.waitForFunction(id=>document.getElementById(id).open,id);
    assert.equal(await page.locator('#'+id+' .reference-body').isVisible(),true,id);
    await page.locator('#'+id).evaluate(el=>el.open=false);
  }
  await page.evaluate(()=>{document.querySelectorAll('.reference-topic').forEach(el=>el.open=true)});
  const imgs=page.locator('.screen-link img');
  for(let i=0;i<await imgs.count();i++){
    await imgs.nth(i).evaluate(async img=>{img.loading='eager';await img.decode()});
    assert.deepEqual(await imgs.nth(i).evaluate(img=>[img.naturalWidth,img.naturalHeight]),[640,480]);
  }
  for(const [name,width,height] of [['desktop',1440,1000],['phone',390,844]]){
    await page.setViewportSize({width,height});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'All references open without page overflow: '+name);
    await page.evaluate(()=>document.querySelectorAll('.reference-topic').forEach(el=>el.open=false));
    await page.goto('file://'+root+'/Choo-Choo-User-Guide.html#instrument-ref-bogie');
    await page.evaluate(()=>document.fonts.ready);
    await page.waitForFunction(()=>document.querySelector('#instrument-ref-bogie').open);
    await page.locator('#instrument-ref-bogie img').evaluate(img=>img.decode());
    await page.locator('#instrument-ref-bogie').evaluate(el=>el.scrollIntoView({block:'start',behavior:'instant'}));
    await page.screenshot({path:root+'/.validation/reference-'+name+'-engine.png'});
    await page.locator('#instrument-ref-bogie .screen-link').click();
    await page.locator('#screen-view img').evaluate(img=>img.decode());
    assert.equal(await page.locator('#screen-view').evaluate(el=>el.open),true);
    await page.keyboard.press('Escape');
    await page.evaluate(()=>location.hash='fx-browser-ref-sequencer-fx');
    await page.waitForFunction(()=>document.querySelector('#fx-browser-ref-sequencer-fx').open);
    await page.locator('#fx-browser-ref-sequencer-fx').evaluate(el=>el.scrollIntoView({block:'start',behavior:'instant'}));
    await page.screenshot({path:root+'/.validation/reference-'+name+'-fx.png'});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'FX table '+name);
    await page.evaluate(()=>{location.hash='top';scrollTo(0,0)});
    await page.screenshot({path:root+'/.validation/reference-'+name+'-top.png'});
  }
  await page.evaluate(()=>window.dispatchEvent(new Event('beforeprint')));
  assert.equal(await page.locator('.reference-topic:not([open])').count(),0);
  await page.evaluate(()=>window.dispatchEvent(new Event('afterprint')));
  assert.ok(await page.locator('.reference-topic:not([open])').count()>0);
  assert.deepEqual(errors,[]);assert.deepEqual(requests,[]);
  const report={referenceTopics:92,screenSections:26,screenshots:await imgs.count(),directLinks:true,offline:true,mobileOverflow:false,printExpandsReferences:true,errors};
  fs.writeFileSync(root+'/.validation/reference-validation.json',JSON.stringify(report,null,2)+'\n');console.log(report);
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
