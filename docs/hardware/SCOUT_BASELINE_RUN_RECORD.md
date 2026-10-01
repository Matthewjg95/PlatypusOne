# Scout baseline run record

Template only. Replace `UNRESOLVED` with actual observations, or leave it
unresolved; an unchecked item is not a PASS. Keep this next to the raw bundle.

| Field | Recorded value |
|---|---|
| Session date/time/timezone and operator | UNRESOLVED |
| Exact source SHA built/tested, plus local changes | UNRESOLVED |
| Kiosk binary SHA256 / build log | UNRESOLVED |
| Kernel / compiler / panel driver version | UNRESOLVED |
| Display connector/mode and touch device | UNRESOLVED |
| Webcam identity/node/mode | UNRESOLVED |
| Reference dimensions / print-scale verification | UNRESOLVED |
| Object identity and verification method | UNRESOLVED |
| Lighting / focus / camera pose / part placement | UNRESOLVED |
| Native build / asserts / synthetic battery | UNRESOLVED |
| Live preview → touch → real capture → card → saved pair | UNRESOLVED |
| Intentional refusal / guidance / saved raw pair | UNRESOLVED |
| Photo/video references / submitted post link, if applicable | UNRESOLVED |
| Sessions FINISH/export exercised? Native bundle/log path | UNRESOLVED — not available in the single-capture baseline |
| Blocker and single next action | UNRESOLVED |

| Capture ID + original JSON/source paths | Condition | Measurand | Caliper truth (mm) | On-device result / refusal | Error (mm) | Class/nominal + unresolved fields |
|---|---|---|---|---|---|---|
| UNRESOLVED | even desk light | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| UNRESOLVED | uneven overhead light | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| UNRESOLVED | moderate shadow | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| UNRESOLVED | rotated placement | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED |
| UNRESOLVED | reference hidden | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED | UNRESOLVED |

Compare the same measurand: overall length, head diameter, shank diameter
and nominal/thread pitch are different facts. Label standard nominal values
separately from actual caliper readings. Preserve original on-device JSON;
put any offline re-analysis in a separate file with its source SHA/method.

- [ ] Original source/image and JSON pairs preserved; refusal records retained.
- [ ] Source SHA and binary hash identify this run, not merely the checkout at collection time.
- [ ] Physical evidence is separated from synthetic fixtures/screenshots.
- [ ] Dimensions/conditions are linked to capture IDs, including repeats.
- [ ] SHA256 manifest verified after copying the bundle.
