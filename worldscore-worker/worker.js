// World high-score API for TRUSS BUSTER ENDLESS.
//   GET  /                        -> { score, initials }   (current world record)
//   POST / {score,initials,token} -> { updated, score, initials }
//
// POSTs must carry token = sha256(SALT + ":" + score + ":" + initials) where SALT
// is a Worker secret shared with the client. This blocks casual curl/devtools
// fakes (you can't forge the token without the salt). It is obscurity, not hard
// security — the salt is embedded in the client wasm and could be extracted by a
// determined attacker; the blast radius is just this one leaderboard value.
const KEY = "record";
const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type",
};
function clean(initials) {
  return ("" + (initials || "")).toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 3).padEnd(3, "-");
}
async function sha256hex(s) {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s));
  return Array.from(new Uint8Array(buf)).map((b) => b.toString(16).padStart(2, "0")).join("");
}
export default {
  async fetch(req, env) {
    if (req.method === "OPTIONS") return new Response(null, { headers: CORS });
    const cur = (await env.SCORES.get(KEY, "json")) || { score: 0, initials: "---" };
    if (req.method === "GET") return Response.json(cur, { headers: CORS });
    if (req.method === "POST") {
      let body = {};
      try { body = await req.json(); } catch (e) {}
      const score = Math.max(0, Math.min(99999999, parseInt(body.score) || 0));
      const initials = clean(body.initials);
      const expect = await sha256hex((env.SCORE_SALT || "") + ":" + score + ":" + initials);
      if (!body.token || body.token !== expect)
        return Response.json({ error: "unauthorized" }, { status: 403, headers: CORS });
      if (score > cur.score) {
        const rec = { score, initials };
        await env.SCORES.put(KEY, JSON.stringify(rec));
        return Response.json({ updated: true, ...rec }, { headers: CORS });
      }
      return Response.json({ updated: false, ...cur }, { headers: CORS });
    }
    return new Response("method not allowed", { status: 405, headers: CORS });
  },
};
