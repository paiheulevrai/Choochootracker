const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
require('node:fs').mkdirSync(path.join(root,'.validation'),{recursive:true});
(async()=>{const browser=await chromium.launch({headless:true,args:['--disable-gpu'],executablePath:process.env.CHROME_EXECUTABLE || undefined});try{
 const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto('file://'+root+'/Choo-Choo-User-Guide.html');await page.evaluate(()=>{document.documentElement.style.scrollBehavior='auto';return document.fonts.ready});
 assert.equal(await page.locator('#cloning table').count(),0);assert.equal(await page.locator('#clone-notes').isVisible(),true);
 for(const [size,width,height] of [['desktop',1440,1100],['phone',390,844]]){
  await page.setViewportSize({width,height});
  for(const choice of ['notes','order','sound']){
   await page.locator('#cloning').evaluate(el=>el.scrollIntoView({block:'start',behavior:'instant'}));
   await page.locator('#clone-tab-'+choice).click();
   assert.equal(await page.locator('.clone-panel:visible').count(),1);
   assert.equal(await page.locator('#clone-'+choice).isVisible(),true);
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
   assert.equal(await page.locator('#clone-'+choice+' .clone-steps li').count(),3);
   await page.locator('#cloning').evaluate(el=>el.scrollIntoView({block:'start',behavior:'instant'}));
   await page.screenshot({path:root+'/.validation/cloning-new-'+size+'-'+choice+'.png'});
   const more=page.locator('#clone-'+choice+' details');await more.locator('summary').click();assert.equal(await more.getAttribute('open')!==null,true);await more.locator('summary').click();
  }
 }
 await page.locator('#clone-tab-order').focus();await page.keyboard.press('ArrowRight');assert.equal(await page.locator('#clone-tab-notes').getAttribute('aria-selected'),'true');
 await page.keyboard.press('End');assert.equal(await page.locator('#clone-tab-sound').getAttribute('aria-selected'),'true');
 await page.keyboard.press('Home');assert.equal(await page.locator('#clone-tab-order').getAttribute('aria-selected'),'true');
 assert.equal(await page.locator('[data-example]').count(),155);assert.deepEqual(errors,[]);
 console.log('PASS: three choices, three steps per choice, expandable detail, desktop/phone layout, keyboard tabs, 155 examples retained, no script errors');
}finally{await browser.close()}})().catch(e=>{console.error(e);process.exit(1)});
