// World high-score API for TRUSS BUSTER ENDLESS.
//   GET  /            -> { score, initials }     (current world record)
//   POST / {score,initials} -> { updated, score, initials }
// One record stored in KV under key "record". Client-submitted (no anti-cheat;
// it's a fun demo), but values are clamped/sanitised.
const KEY = "record";
const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type",
};
function clean(initials) {
  return ("" + (initials || "")).toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 3).padEnd(3, "-");
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
