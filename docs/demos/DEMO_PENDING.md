# M1 First Voice Demo — Pending

This stub is a placeholder for the M1 First Voice demo video.

## Recording Instructions

1. Start PS-002 Server: `cd ~/Documents/Github/Pairion-Server && pnpm dev`
2. Start Client debug build: `cd ~/Documents/Github/Pairion-Client && pnpm tauri dev`
3. Wait for "Connected" status in the Client window
4. Say "Hey Pairion, what's the weather?"
5. Observe state transitions: Listening → Thinking → Speaking
6. Listen for the British voice response
7. Record the screen and audio (~30–60 seconds)

## Save As

`m1-first-voice.mov` (or `.mp4`) in this directory.

## What to Capture

- Client window showing "Connected" status
- Voice-state indicator transitions (Listening/Thinking/Speaking badges)
- Transcript strip showing partial and final text
- Audible British voice response through speaker
- Console or overlay showing measured wake-to-first-TTS-byte latency
