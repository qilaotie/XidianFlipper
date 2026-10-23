import test from 'node:test';
import assert from 'node:assert/strict';
import gateway from '../api-gateway/public/_worker.js';
test('Pages gateway uses only a fixed service binding, preserving auth/body',async()=>{
  const request=new Request('https://xidianflipper-api.pages.dev/v1/sync',{method:'POST',headers:{Authorization:'Bearer test-not-a-real-key'},body:'{"test":true}'});
  let calls=0;const env={COMMUNITY:{fetch:async r=>{calls++;assert.equal(r,request);assert.equal(r.headers.get('Authorization'),'Bearer test-not-a-real-key');assert.equal(await r.text(),'{"test":true}');return new Response('ok');}}};
  assert.equal(await (await gateway.fetch(request,env)).text(),'ok');
  assert.equal((await gateway.fetch(new Request('https://xidianflipper-api.pages.dev/?url=https://evil.example'),env)).status,404);
  assert.equal(calls,1);
});
