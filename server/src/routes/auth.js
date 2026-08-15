const express = require('express');
const { verifyFirebaseIdToken } = require('../firebaseAuth');
const { pool } = require('../db');

const router = express.Router();

// POST /auth/firebase/login  { id_token }
// Verifies the Firebase ID token, upserts a users row keyed on firebase_uid, and returns the
// canonical user record. Must be called once after Firebase sign-in/sign-up before any other
// route will accept the token (requireAuth rejects tokens with no matching users row).
router.post('/firebase/login', async (req, res, next) => {
  const { id_token: idToken } = req.body || {};
  if (!idToken) {
    return res.status(400).json({ error: 'id_token is required' });
  }

  let decoded;
  try {
    decoded = await verifyFirebaseIdToken(idToken);
  } catch (err) {
    return res.status(401).json({ error: 'Invalid or expired token' });
  }

  try {
    const { rows } = await pool.query(
      `INSERT INTO users (email, firebase_uid)
       VALUES ($1, $2)
       ON CONFLICT (firebase_uid)
       DO UPDATE SET email = EXCLUDED.email
       RETURNING id, email, firebase_uid, created_at, is_active`,
      [decoded.email || null, decoded.sub]
    );

    res.json(rows[0]);
  } catch (err) {
    next(err);
  }
});

module.exports = router;
