const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
require('node:fs').mkdirSync(path.join(root,'.validation'),{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--disable-gpu'],executablePath:process.env.CHROME_EXECUTABLE || undefined});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.goto('file://'+root+'/Choo-Choo-User-Guide.html');
  await page.evaluate(()=>{document.documentElement.style.scrollBehavior='auto';return document.fonts.ready});
  const goto=async id=>{
   await page.locator('#'+id).evaluate(el=>el.scrollIntoView({block:'start',behavior:'instant'}));
   await page.waitForFunction(id=>document.querySelector('.nav-section-link[aria-current]')?.hash==='#'+id,id);
  };
  await goto('instrument');
  assert.equal(await page.locator('.nav-topics:not([hidden])').count(),1);
  assert.equal(await page.locator('#contents-instrument').isVisible(),true);
  assert.equal(await page.locator('#contents-instrument a').first().textContent(),'Sideloaded presets');
  assert.equal(await page.locator('#instrument .reference-topic>summary').first().textContent(),'Sideloaded presets');
  assert.equal(await page.locator('#instrument-ref-soundfont>summary').textContent(),'SoundFont');
  const target=page.locator('#contents-instrument a').first();
  await target.click();
  await page.waitForFunction(()=>document.getElementById('instrument-ref-sideloaded-presets').open);
  await page.waitForFunction(()=>document.querySelector('[data-topic="instrument-ref-sideloaded-presets"]').getAttribute('aria-current')==='true');
  const parentBox=await page.locator('.nav-section-link[href="#instrument"]').boundingBox();
  const contentsBox=await page.locator('#contents').boundingBox();
  assert.ok(parentBox.y>=contentsBox.y-1,'Active parent stays visible above its topics');
  await page.screenshot({path:root+'/.validation/navigation-desktop-presets.png'});
  // Reusing an existing hash must reopen a manually closed topic.
  await page.locator('#instrument-ref-sideloaded-presets>summary').click();
  await target.click();
  await page.waitForFunction(()=>document.getElementById('instrument-ref-sideloaded-presets').open);
  // Scrolling the sidebar does not scroll the article.
  const before=await page.evaluate(()=>scrollY);
  await page.locator('#contents').evaluate(el=>el.scrollTop+=120);
  assert.ok(Math.abs(await page.evaluate(()=>scrollY)-before)<2);
  // Scroll to another page: the previous submenu closes and the new one appears.
  await goto('modulation');
  assert.equal(await page.locator('#contents-instrument').isVisible(),false);
  assert.equal(await page.locator('#contents-modulation').isVisible(),true);
  await page.locator('#contents-modulation a').first().focus();await page.keyboard.press('Enter');
  await page.waitForFunction(()=>document.getElementById('modulation-ref-sources-destinations-and-lfos').open);
  await goto('groove');assert.equal(await page.locator('.nav-topics:not([hidden])').count(),0);
  // Mobile keeps its compact contents layout, avoiding height changes above the article.
  await page.setViewportSize({width:390,height:844});
  await goto('instrument');
  assert.equal(await page.locator('.nav-topics:not([hidden])').count(),0);
  const position=await page.evaluate(()=>scrollY);
  await page.evaluate(()=>{dispatchEvent(new Event('scroll'));dispatchEvent(new Event('resize'))});
  await page.waitForTimeout(100);
  assert.ok(Math.abs(await page.evaluate(()=>scrollY)-position)<2);
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
  await page.screenshot({path:root+'/.validation/navigation-phone.png'});
  // Returning to a sidebar viewport expands the active section again.
  await page.setViewportSize({width:1440,height:1000});await goto('instrument');
  await page.waitForFunction(()=>!document.querySelector('#contents-instrument').hidden);
  assert.equal(await page.locator('.page').count(),26);
  assert.equal(await page.locator('[data-example]').count(),155);
  assert.deepEqual(errors,[]);
  const report={automaticExpansion:true,oneActiveSubmenu:true,topicHighlight:true,keyboard:true,sameHashReopens:true,independentSidebarScroll:true,mobileStable:true,screenSections:26,gifExamples:155,errors};
  fs.writeFileSync(root+'/.validation/navigation-validation.json',JSON.stringify(report,null,2)+'\n');console.log(report);
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
