CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  token_hash TEXT NOT NULL,
  nickname TEXT NOT NULL,
  pet TEXT NOT NULL,
  updated_at INTEGER NOT NULL,
  visible INTEGER NOT NULL DEFAULT 1 CHECK(visible IN (0,1))
);
CREATE INDEX IF NOT EXISTS devices_recent ON devices(visible, updated_at DESC, id);
CREATE TABLE IF NOT EXISTS scores (
  device_id TEXT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  game TEXT NOT NULL CHECK(game IN ('snake','tetris')),
  score INTEGER NOT NULL CHECK(score >= 0),
  progress INTEGER NOT NULL CHECK(progress >= 0),
  achieved_at INTEGER NOT NULL,
  PRIMARY KEY(device_id, game)
);
CREATE INDEX IF NOT EXISTS scores_rank ON scores(game, score DESC, achieved_at ASC, device_id);
