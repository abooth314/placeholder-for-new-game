# Procedural presentation recipes

All game models and audio are generated from versioned JSON recipes. There are
no imported finished models, sprite sheets, raster game-art textures or recorded
sounds. ImGui's licensed embedded outline font is rasterized into an atlas at
runtime; it is not a game-art authoring input.

visuals records have a namespaced id and 1..160 parts. Every part has primitive,
center, size, color, optional tint, motion and sides. Lengths are local physical
metres and colors are [0,1] linear-style RGB values. Centers/endpoints lie within
[-20,20], size components in [.005,30], tint in [0,1], sides in [3,16].

| Evaluator | Parameters / meaning |
| --- | --- |
| box | center and full width/height/depth |
| cone | center at base; size=[bottom radius,height,top radius] |
| roof | center at eaves; size=[width,height,length] |
| beam | center=start, end=endpoint, size[0]=radius |
| ellipsoid | center, size as three radii; four latitude bands |

Parts produce solid triangle meshes with normals, team tint and motion tags.
Tag 0 is static, 1/2 are opposite leg motion and 3 is attack motion. Native
evaluators use the actor's actual walk distance/attack flash; these animations
never change hit results. Long edges subdivide above 3.2 m, to at most five
levels; each generated recipe is capped at 120,000 vertices. Unsupported new
geometry/animation evaluators need native work. New compatible combinations of
the existing evaluators do not.

Models are cached by BaseId for the immutable catalog lifetime and uploaded
once. Each model retains one vertex buffer, one instance buffer and one VAO,
with at most 1024 submitted instances per model. The terrain mesh/atlas rebuild
only at map replacement. Camera-relative terrain coordinates update each frame;
the terrain topology and authored meshes are not regenerated. The atlas is a
generated 256x256 texture. Context replacement destroys/recreates GPU ownership;
window resize changes the viewport without rebuilding unchanged meshes.

Low detail reduces decorative tree/effect submissions. It does not change
simulation RNG, movement, populations, projectiles or hit results. Props are
solid H³ geometry but currently have no general swept collision consumer.
Trees/roofs/castles and a few tethered horses are recognizable procedural models;
mounted combat and its movement animations remain deferred.

sounds records specify duration [.02,2] s, initial/end frequency [15,12000] Hz,
noise/tone/gain [0,1] and priority [0,10]. The reusable evaluator combines a
frequency glide, harmonic sine, deterministic noise and a bounded envelope at
48 kHz. Each record is synthesized/cached once. Melee, bow, siege, impact,
healing and collapse have live combat consumers. Hoof and magic recipes are
also generated and included in the audio demo; their corresponding mounted/mage
combat roles are deferred.

The SDL mixer streams stereo float audio. It maintains up to 24 priority
voices, equal-power panning from the listener's H³ tangent frame, and distance
attenuation to 150 m. Completed voices are removed. Requests within 35 ms for
the same sound are coalesced; when full, a higher-priority request replaces a
lower one. It generates quiet filtered wind and occasional bird chirps through
the ambience bus. Master/effects/ambience/mute are independent persisted settings.

Windows tries the default SDL audio driver, then DirectSound when opening that
device fails and no SDL_AUDIO_DRIVER override was supplied. Failure is visible
in the inspector/council, never replaced by an assertion that silence passed.
Offline waveform, native-device, generated-stream and human listening evidence
are distinct and reported separately in STATUS.md.
