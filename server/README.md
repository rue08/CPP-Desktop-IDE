# vw-backend

Node.js + PostgreSQL backend for file storage, replacing Firebase Firestore/Storage. **Firebase
Authentication is unchanged** — the C++ client still signs up/in/refreshes against Firebase
directly. This server only verifies the Firebase ID tokens the client already has, using
Google's public JWKS (no Firebase Admin SDK, no service account secret).

## Run it

```sh
cp .env.example .env   # defaults are fine for local use
docker compose up --build
```

This starts Postgres (schema applied automatically from `migrations/001_init.sql` on first
boot) and the API on `http://localhost:3000`.

To expose it publicly for the client (e.g. via ngrok), point the tunnel at port 3000. The
backend URL is not hardcoded anywhere in the C++ client — it's meant to be configurable, since
ngrok's free tier gives a new URL on every restart.

## API

- `POST /auth/firebase/login` `{ id_token }` — call once after Firebase sign-in/sign-up;
  upserts a `users` row and returns it. Required before any other route will accept the token.
- `POST /files` `{ filename, content }` — create or update (keyed on filename) a file for the
  caller.
- `GET /files` — list the caller's files (metadata only).
- `GET /files/:id` — fetch one file's content (ownership-checked).
- `DELETE /files/:id` — delete one file (ownership-checked).
- `DELETE /account` — deletes the caller's `users` row (cascades to their files). Call this
  *before* calling Firebase's own `accounts:delete` endpoint — this call still needs a valid
  token.

All routes except `/health` and `/auth/firebase/login` require `Authorization: Bearer <Firebase
ID token>`.

## Local dev without Docker

```sh
npm install
# requires a local Postgres reachable at DB_URL; run migrations/001_init.sql against it once
npm run dev
```

## Out of scope (deliberately, for this pass)

- Rate limiting / abuse prevention.
- Any auth logic beyond verifying Firebase-issued tokens (no passwords, no JWT issuance).
