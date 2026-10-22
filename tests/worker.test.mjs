import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {readFileSync} from 'node:fs';
import worker,{validate} from '../backend/worker.mjs';
const sample=()=>({id:'1'.repeat(32),nickname:'Player_1',pet:{age:1801,form:1,coins:30,dex:1,decor:0,fullness:80,mood:90,energy:100,clean:70,sleeping:false},scores:[{game:'snake',score:4,progress:7},{game:'tetris',score:120,progress:1}]});
test('strict validation and privacy whitelist',()=>{
  assert.ok(validate(sample()));
  for(const change of [d=>d.pet.fullness=101,d=>d.pet.sleeping=1,d=>d.nickname='<script>',d=>d.scores[0].score=-1,d=>d.scores[0].progress=9,d=>d.scores.push(d.scores[0]),d=>d.id='MAC:123']){const d=sample();change(d);assert.equal(validate(d),null);}
  const d=sample();d.password='never-store';d.pet.address='not-public';assert.equal(JSON.stringify(validate(d)).includes('never-store'),false);assert.equal('address' in validate(d).pet,false);
});
test('production SQL: enrollment, ownership, best, privacy and limits',async()=>{
  const sqlite=new DatabaseSync(':memory:');sqlite.exec('PRAGMA foreign_keys=ON');sqlite.exec(readFileSync(new URL('../backend/migrations/0001_init.sql',import.meta.url),'utf8'));
  class Statement{constructor(sql,args=[]){this.sql=sql;this.args=args;}bind(...args){return new Statement(this.sql,args);}first(){return sqlite.prepare(this.sql).get(...this.args)||null;}all(){return {results:sqlite.prepare(this.sql).all(...this.args)};}run(){return sqlite.prepare(this.sql).run(...this.args);}}
  const env={DB:{prepare:sql=>new Statement(sql),batch:async statements=>{sqlite.exec('BEGIN');try{const out=statements.map(s=>s.run());sqlite.exec('COMMIT');return out;}catch(e){sqlite.exec('ROLLBACK');throw e;}}},PUBLIC_ORIGIN:'https://qilaotie.github.io',MAX_DEVICES:'2',WRITE_LIMIT:{limit:async()=>({success:true})},READ_LIMIT:{limit:async()=>({success:true})}};
  globalThis.caches={default:{match:async()=>null,put:async()=>{}}};const ctx={waitUntil:()=>{}};
  const call=(method,path,data,token='a'.repeat(64),headers={})=>worker.fetch(new Request('https://api.example'+path,{method,headers:{'Content-Type':'application/json',Authorization:'Bearer '+token,...headers},...(data===undefined?{}:{body:JSON.stringify(data)})}),env,ctx);
  const d=sample();
  assert.equal((await call('POST','/v1/sync',d,'bad')).status,401);
  assert.equal((await call('POST','/v1/sync',d,'a'.repeat(64),{Origin:'https://evil.example'})).status,403);
  assert.equal((await call('POST','/v1/sync',{...d,nickname:'x'.repeat(2100)})).status,413);
  assert.equal((await call('POST','/v1/sync',d)).status,200);
  assert.equal((await call('POST','/v1/sync',d,'b'.repeat(64))).status,401);
  assert.equal((await call('POST','/v1/sync',d)).status,429);
  let board=await (await call('GET','/v1/leaderboard?game=snake')).json();
  assert.equal(board.items[0].score,4);assert.equal('token_hash' in board.items[0],false);
  sqlite.exec('UPDATE devices SET updated_at=0');d.scores[0].score=2;d.scores[0].progress=5;
  assert.equal((await call('POST','/v1/sync',d)).status,200);
  assert.equal((await (await call('GET','/v1/leaderboard?game=snake')).json()).items[0].score,4);
  sqlite.exec('UPDATE devices SET updated_at=0');d.scores[0].score=5;d.scores[0].progress=8;
  assert.equal((await call('POST','/v1/sync',d)).status,200);
  assert.equal((await (await call('GET','/v1/leaderboard?game=snake')).json()).items[0].score,5);
  assert.equal((await (await call('GET','/v1/pets')).json()).items[0].pet.form,1);
  const second=sample();second.id='2'.repeat(32);assert.equal((await call('POST','/v1/sync',second)).status,200);
  const third=sample();third.id='3'.repeat(32);assert.equal((await call('POST','/v1/sync',third)).status,503);
  assert.equal((await call('DELETE','/v1/sync',{id:d.id},'b'.repeat(64))).status,401);
  assert.equal((await call('DELETE','/v1/sync',{id:d.id})).status,200);
  assert.equal((await (await call('GET','/v1/pets')).json()).items.length,1);
  assert.equal(sqlite.prepare('SELECT COUNT(*) AS n FROM scores WHERE device_id=?').get(d.id).n,0);
  assert.equal((await call('GET','/v1/leaderboard?game=no')).status,400);
  env.WRITE_LIMIT.limit=async()=>({success:false});assert.equal((await call('POST','/v1/sync',second)).status,429);
  env.READ_LIMIT.limit=async()=>({success:false});assert.equal((await call('GET','/v1/pets')).status,429);
  sqlite.close();
});
