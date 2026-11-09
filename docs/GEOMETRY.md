# Spatial and numerical contract

Coordinates are unit-hyperboloid points P=(t,x,y,z), t>0,
-t²+x²+y²+z²=-1. Sectional curvature is -1/L². Physical distances are metres;
tangent vector norms are metres. exp(P,V) uses cosh(|V|/L) P +
sinh(|V|/L) V/|V|. log returns a physical tangent vector. No flat plane owns
simulation positions, collision ranges, sound positions or camera orientation.

Distance uses 2 L asinh(sqrt(<P-Q,P-Q>)/2) to preserve close-point precision.
Dot products accumulate in long double before returning double. Unit frames are
constructed through H² transport and the normal lift, not a screen transform.
Parallel transport is along the ambient geodesic. The exposed reflection
isometry preserves Lorentz products and may reverse orientation.

Input points must be finite, future directed, t<cosh(12), with hyperboloid
residual below 2e-8 (1+t²). Invalid points, chart boundary values and invalid
curvature are rejected. Space supports L in [4,256] m; the client exposes
[30,100] m. Terrain radius/L must be <=3.5. Individual exp steps are bounded
by 12 curvature radii. These are finite numerical domains, not claims of an
unbounded exact world. The spectator camera is bounded to 180 m from origin.

Independent tests check known radial distances down to 1e-8 m, 5L separation,
the hyperbolic right-triangle cosine law, lift height, transport, frame
orthogonality, isometry and invalid inputs at L=8,48,160. Specific assertion
tolerances are in tests/domain_tests.cpp; graphics uses float after a double
precision transformation to the camera frame.

Terrain is a smooth normal displacement h(B) of an H² slice:
P=cosh(h/L) B + sinh(h/L) U. Its induced line element is
ds²=dh²+cosh²(h/L) ds_H²². The two versioned map recipes parameterize native
height/river evaluators. River placement uses intrinsic H² Fermi coordinates.
Bridges and their graded approaches use the same height function as actors.
Water below 0.35 m and slopes of 0.95 or more are not traversable.

Navigation samples a 73x73 Klein lattice clipped to the intrinsic map disk.
Edges are legal only after continuous recipe checks at <=0.35 m spacing.
Surface length is accumulated from short ambient chords on the **lifted**
curve, with at least eight panels and normally <=0.5 m spacing. A* uses ambient
H³ distance as a lower bound, not chart distance. Components are explicit;
spawns and graph goals resolve in the fortress's component. Snapping is a
documented local chart policy, not a promise of globally exact nearest points.

Movement bisects physical surface length (12 iterations, at least four panels)
to fit speed*1/30 s. It checks the actual continuous segment before committing.
Local separation may steer only along a legal segment. Shortcuts never cross
blocked water or slopes. The finite graph can reject a very thin route which
exists in the continuous recipe. Full swept capsule collision against trees,
houses and walls is deferred; these props are currently decorative geometry.

The nearby grid is a conservative broad phase. Fermi projection to H² is
1-Lipschitz; the Klein metric is >= L² times the chart Euclidean metric.
Consequently a chart radius r/L contains every surface actor within r metres.
Candidates are filtered by actual H³ distance. The grid is rebuilt before
planning and after movement, so no historical-position padding is assumed.

Projectiles follow an explicitly chosen game-gravity profile: interpolate the
base H² geodesic, then normally lift to
h(t)=(1-t)h0+t h1+4 A t(1-t). This is an airborne H³ trajectory, not Newtonian
ambient gravity. Each step is subdivided into <=0.25 m ambient chords for
terrain/body checks; collision is an approximation to that sampled trajectory.

Mesh recipes describe local physical tangents, exponentiated into H³ in the
GPU. Terrain is already embedded. The CPU rebases points and frames to the
camera in double precision. Perspective uses qx/qz and qy/qz in this
orthonormal Lorentz frame; depth uses projected Klein z=qz/qt, with a 0.2 m
near distance and a 280 m forward-axis far bound. Static triangles and long
model edges are controlled tessellation approximations. Lighting uses local
normals; no claim of exact global illumination is made.
