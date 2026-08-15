const express = require('express');
const { pool } = require('../db');
const { requireAuth } = require('../middleware/auth');

const router = express.Router();

router.use(requireAuth);

// POST /files  { filename, content }
// Create-or-update semantics, keyed on (user_id, filename) -- mirrors the one-document-per-
// filename model the client already uses against Firestore in storage.cpp.
router.post('/', async (req, res, next) => {
  const { filename, content } = req.body || {};
  if (!filename) {
    return res.status(400).json({ error: 'filename is required' });
  }
  if (!filename.toLowerCase().endsWith('.cpp')) {
    return res.status(400).json({ error: 'Only .cpp files can be uploaded' });
  }

  try {
    const updated = await pool.query(
      `UPDATE files SET content = $1, updated_at = now()
       WHERE user_id = $2 AND filename = $3
       RETURNING id, filename, content, updated_at`,
      [content ?? '', req.userId, filename]
    );

    if (updated.rowCount > 0) {
      return res.json(updated.rows[0]);
    }

    const inserted = await pool.query(
      `INSERT INTO files (user_id, filename, content)
       VALUES ($1, $2, $3)
       RETURNING id, filename, content, updated_at`,
      [req.userId, filename, content ?? '']
    );

    res.status(201).json(inserted.rows[0]);
  } catch (err) {
    next(err);
  }
});

// GET /files -- list the caller's files (metadata only, no content, to keep the listing light).
router.get('/', async (req, res, next) => {
  try {
    const { rows } = await pool.query(
      'SELECT id, filename, updated_at FROM files WHERE user_id = $1 ORDER BY updated_at DESC',
      [req.userId]
    );
    res.json(rows);
  } catch (err) {
    next(err);
  }
});

// GET /files/:id -- download one file's content, verifying ownership.
router.get('/:id', async (req, res, next) => {
  try {
    const { rows } = await pool.query(
      'SELECT id, filename, content, updated_at FROM files WHERE id = $1 AND user_id = $2',
      [req.params.id, req.userId]
    );
    if (rows.length === 0) {
      return res.status(404).json({ error: 'Not found' });
    }
    res.json(rows[0]);
  } catch (err) {
    next(err);
  }
});

// DELETE /files/:id -- verifies ownership before deleting.
router.delete('/:id', async (req, res, next) => {
  try {
    const { rowCount } = await pool.query(
      'DELETE FROM files WHERE id = $1 AND user_id = $2',
      [req.params.id, req.userId]
    );
    if (rowCount === 0) {
      return res.status(404).json({ error: 'Not found' });
    }
    res.status(204).end();
  } catch (err) {
    next(err);
  }
});

module.exports = router;
