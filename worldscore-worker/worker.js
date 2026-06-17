// World leaderboard API for TRUSS BUSTER.
//   GET  /                        -> { score, initials, board:[{score,initials}..] }
//   POST / {score,initials,token} -> same shape after inserting
//
// Keeps the top BOARD_MAX scores in KV (key "board"). `score`/`initials` echo the
// #1 entry for the simple HI WORLD display. POSTs must carry
// token = sha256(SALT + ":" + score + ":" + initials); the Worker rejects any
// request without a valid token (403), blocking casual curl/devtools fakes. The
// salt is a Worker secret shared with the client — obscurity, not hard security.
const KEY = "board";
const BOARD_MAX = 10;
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
function withTop(board) {
  return { score: board[0] ? board[0].score : 0, initials: board[0] ? board[0].initials : "---", board };
}
export default {
  async fetch(req, env) {
    if (req.method === "OPTIONS") return new Response(null, { headers: CORS });
    let board = (await env.SCORES.get(KEY, "json")) || [];
    if (req.method === "GET") return Response.json(withTop(board), { headers: CORS });
    if (req.method === "POST") {
      let body = {};
      try { body = await req.json(); } catch (e) {}
      const score = Math.max(0, Math.min(99999999, parseInt(body.score) || 0));
      const initials = clean(body.initials);
      const expect = await sha256hex((env.SCORE_SALT || "") + ":" + score + ":" + initials);
      if (!body.token || body.token !== expect)
        return Response.json({ error: "unauthorized" }, { status: 403, headers: CORS });
      if (score > 0) {
        board.push({ score, initials });
        board.sort((a, b) => b.score - a.score);
        board = board.slice(0, BOARD_MAX);
        await env.SCORES.put(KEY, JSON.stringify(board));
      }
      return Response.json(withTop(board), { headers: CORS });
    }
    return new Response("method not allowed", { status: 405, headers: CORS });
  },
};
