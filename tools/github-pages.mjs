// Uses the OS Git credential helper in memory. Never prints or persists credentials.
import {execFileSync} from 'node:child_process';
const credential=execFileSync('git',['-c','http.sslVerify=true','credential','fill'],{input:'protocol=https\nhost=github.com\n\n',encoding:'utf8',env:{...process.env,GIT_TERMINAL_PROMPT:'0',GCM_INTERACTIVE:'Never'}});
const fields=Object.fromEntries(credential.trim().split('\n').map(line=>{const i=line.indexOf('=');return [line.slice(0,i),line.slice(i+1)];}));
if(!fields.password)throw Error('GitHub authorization missing');
const api='https://api.github.com/repos/qilaotie/XidianFlipper/pages';
const headers={Accept:'application/vnd.github+json',Authorization:'Bearer '+fields.password,'X-GitHub-Api-Version':'2022-11-28','User-Agent':'XidianFlipper-deploy'};
let response=await fetch(api,{headers});
if(response.status===404 && process.argv.includes('--enable'))response=await fetch(api,{method:'POST',headers:{...headers,'Content-Type':'application/json'},body:JSON.stringify({source:{branch:'gh-pages',path:'/'},build_type:'legacy'})});
if(!response.ok)throw Error('GitHub Pages API status '+response.status);
const data=await response.json();
console.log(JSON.stringify({url:data.html_url,status:data.status,source:data.source,https_enforced:data.https_enforced},null,2));
if(data.source?.branch!=='gh-pages')throw Error('Existing Pages source differs; preserved without changes');
