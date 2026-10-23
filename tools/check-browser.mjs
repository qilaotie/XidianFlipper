import {createRequire} from 'node:module';
const {chromium}=createRequire(import.meta.url)(process.env.PLAYWRIGHT_MODULE||'playwright');
import {mkdir} from 'node:fs/promises';
import assert from 'node:assert/strict';
await mkdir('test-results',{recursive:true});
let browser;try{browser=await chromium.launch({headless:true});}catch{browser=await chromium.launch({channel:'msedge',headless:true});}
try{
  const page=await browser.newPage({viewport:{width:1440,height:1100}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.goto(process.env.PREVIEW_URL||'http://127.0.0.1:8080',{waitUntil:'networkidle'});
  await page.getByText(/已连接小岛/).waitFor({timeout:20000});
  await page.screenshot({path:'test-results/desktop.png',fullPage:true});
  await page.getByRole('tab',{name:/俄罗斯方块/}).click();
  await page.getByText('俄罗斯方块 · 最高分').waitFor();
  assert.equal(await page.getByRole('tab',{name:/俄罗斯方块/}).getAttribute('aria-selected'),'true');
  await page.setViewportSize({width:390,height:844});
  const overflow=await page.evaluate(()=>[...document.querySelectorAll('body *')].filter(e=>e.getBoundingClientRect().right>innerWidth+1).map(e=>({tag:e.tagName,cls:e.className,right:e.getBoundingClientRect().right})));
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,JSON.stringify(overflow));
  await page.screenshot({path:'test-results/mobile.png',fullPage:true});
  // Browser-only fixtures: never uploaded to the public database.
  const fixture={id:'f'.repeat(32),nickname:'<img onerror=alert(1)>',score:4,progress:7,updated_at:1700000000,pet:{age:1801,form:1,coins:30,dex:1,decor:0,fullness:80,mood:90,energy:100,clean:70,sleeping:false}};
  const apiPattern=/https:\/\/[^/]+\.(workers|pages)\.dev\/v1\//;
  await page.route(apiPattern,r=>r.fulfill({json:{items:[]}}));
  await page.getByRole('button',{name:/刷新数据/}).click();
  await page.getByText('展示柜还空着，等第一位小伙伴来报到。').waitFor();
  assert.equal(await page.locator('.pet-card').count(),0);
  await page.unrouteAll({behavior:'wait'});
  await page.route(apiPattern,r=>r.fulfill({json:{items:[fixture]}}));
  await page.getByRole('button',{name:/刷新数据/}).click();
  await page.getByRole('heading',{name:fixture.nickname,exact:true}).waitFor();
  assert.equal(await page.locator('.pet-card img').count(),0);
  assert.ok((await page.locator('.pet-card h3').textContent()).includes('<img'));
  assert.equal(await page.locator('.meter-fill').first().evaluate(e=>e.style.width),'80%');
  await page.unrouteAll({behavior:'wait'});
  await page.route(apiPattern,r=>r.abort());
  await page.getByRole('button',{name:/刷新数据/}).click();
  await page.getByText('连接暂不可用，可稍后重试').waitFor();
  assert.equal(await page.getByRole('button',{name:/刷新数据/}).isEnabled(),true);
  assert.deepEqual(errors,[]);
  console.log('PASS: real API, empty state, tabs, mobile overflow, text-only rendering, meters, network error; no database fixtures written.');
}finally{await browser.close();}
