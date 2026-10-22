// Creates one clearly marked temporary test profile, then hides it. No secrets printed.
import {randomBytes} from 'node:crypto';
import assert from 'node:assert/strict';
import {API_BASE} from '../config.js';
const id=randomBytes(16).toString('hex'),token=randomBytes(32).toString('hex');
const body={id,nickname:'API_CHECK',pet:{age:0,form:0,coins:0,dex:0,decor:0,fullness:80,mood:80,energy:100,clean:100,sleeping:false},scores:[]};
async function call(method,payload,credential=token){return fetch(API_BASE+'/v1/sync',{method,headers:{'Content-Type':'application/json',Authorization:'Bearer '+credential},body:JSON.stringify(payload),signal:AbortSignal.timeout(20000)});}
let created=false;
try{
  const upload=await call('POST',body);assert.equal(upload.status,200,await upload.text());created=true;
  const wrong=await call('POST',body,'b'.repeat(64));assert.equal(wrong.status,401);
  const frequent=await call('POST',body);assert.equal(frequent.status,429);
  console.log('PASS: live HTTPS upload / wrong-token rejection / per-device cooldown.');
}finally{
  if(created){const hidden=await call('DELETE',{id});assert.equal(hidden.status,200,await hidden.text());console.log('Temporary API_CHECK profile hidden; cleanup_id='+id);}
}
