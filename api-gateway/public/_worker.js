// Only a provider-internal service binding. No arbitrary URL forwarding.
export default {
  fetch(request,env) {
    const path=new URL(request.url).pathname;
    if(path!=='/health' && !path.startsWith('/v1/'))return new Response('Not found',{status:404});
    return env.COMMUNITY.fetch(request);
  }
};
