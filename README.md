# Mini Path Tracer

Un path tracer écrit from scratch en C++17, sans bibliothèque graphique (seule `stb_image` sert à lire les images HDR). Rendu physique par Monte Carlo, plusieurs matériaux (diffus, métal, verre), milieux participants (brouillard), éclairage par image (IBL) et rendu multithreadé.

![Rendu final](render_ibl.png)

## Fonctionnalités

- **Rendu par path tracing** : simulation physique de la lumière par Monte Carlo (rebonds aléatoires, moyennés sur plusieurs échantillons par pixel)
- **Matériaux** :
  - Diffus (Lambertian)
  - Métal avec effet de flou réglable (fuzz)
  - Verre (Dielectric) avec réflexion/réfraction selon l'angle (approximation de Schlick)
- **Milieux participants** : sphère de brouillard à densité constante, loi de Beer-Lambert, diffusion isotrope
- **Éclairage par image (IBL)** : carte d'environnement HDR équirectangulaire comme source de lumière
- **Caméra configurable** : position, cible, champ de vision (field of view)
- **Anti-aliasing** par supersampling
- **Rendu multithreadé** : parallélisation sur tous les cœurs CPU disponibles (mesuré : 5,4 s avec 1 thread, 0,88 s avec 8 threads, soit environ 6x, à 400x225 et 100 échantillons)

## Compilation

Prérequis : CMake 3.10+, compilateur C++17 (g++/clang)

```bash
mkdir build && cd build
cmake ..
make
./path_tracer
```

Génère une image `output.ppm` dans le dossier `build/`.

## Utilisation

```bash
./path_tracer [échantillons] [threads] [envmap.hdr] [intensité] [rotation en degrés] [borne par échantillon]
```

Exemples :

```bash
./path_tracer 300                                   # ciel en dégradé, 300 échantillons
./path_tracer 400 0 ../data/kloofendal_48d_partly_cloudy_puresky_1k.hdr   # éclairage par image
```

`0` pour les threads = autant que de cœurs détectés.

## Structure du projet

```
src/
├── main.cpp        # Boucle de rendu multithreadée, scène et caméra
├── vec3.h          # Classe vecteur 3D
├── rng.h           # Générateur aléatoire par thread
├── ray.h           # Rayon (origine + direction)
├── hit_record.h    # Description d'une intersection
├── sphere.h        # Intersection rayon/sphère
├── material.h      # Matériaux : Lambertian, Metal, Dielectric
├── volume.h        # Milieu participant homogène (brouillard)
├── envmap.h        # Carte d'environnement HDR (IBL)
└── stb_image.h     # Chargeur d'images (domaine public)
data/               # Cartes d'environnement HDR
```

## Milieux participants (brouillard / fumée)

![Boule de brouillard devant la scène](render_volume.png)

Une sphère de brouillard à densité constante, traversée par les rayons.

- **Loi de Beer-Lambert** : sur une distance d dans le milieu, la fraction de lumière qui traverse sans interagir vaut exp(-densité × d).
- **Implémentation Monte Carlo** : pour chaque rayon, on tire une distance d'interaction -ln(u) / densité. Si elle est plus courte que la distance parcourue dans la sphère, le rayon est diffusé dans une direction uniforme ; sinon il traverse. Cette probabilité de traversée a été vérifiée numériquement (2 millions de rayons : 0,0183 mesuré pour 0,0183 attendu).
- **Réglages** : densité (opacité du milieu) et couleur de diffusion (blanc pour du brouillard, sombre pour de la fumée), dans `volumes.push_back(Volume(...))` de `main.cpp`.

Limites : densité constante, diffusion isotrope, volumes sphériques uniquement, pas d'échantillonnage direct des lumières (donc du bruit à faible nombre d'échantillons).

## Éclairage par image (IBL)

![Scène éclairée par une envmap HDR](render_ibl.png)

La scène est éclairée par une carte d'environnement HDR (image équirectangulaire) au lieu d'un simple dégradé de ciel : quand un rayon s'échappe de la scène, il lit sa couleur dans l'image. Le brouillard est éclairé par la même envmap.

- **Chargement** : images `.hdr` (Radiance) via `stb_image`, interpolation bilinéaire, projection équirectangulaire.
- **Envmap de la capture** : Kloofendal 48d Partly Cloudy (Pure Sky), 1K, Poly Haven (licence CC0).

Limites : l'échantillonnage est simple (les rayons ne trouvent le soleil que par hasard), donc un soleil très fort donne du bruit. Chaque échantillon est borné pour éviter les points blancs, ce qui retire une partie de l'énergie du soleil. Piste d'amélioration : échantillonnage d'importance de l'envmap.

## Ce que ce projet démontre

Projet réalisé en autonomie en parallèle de mon M2 GIG (Géométrie Informatique Graphique) à Aix-Marseille Université, pour approfondir les fondamentaux mathématiques du rendu 3D (vecteurs, intersections géométriques, éclairage physique, rendu de volumes) avant d'attaquer un moteur de rendu temps réel (OpenGL/Vulkan).

Inspiré de la méthode décrite dans [Ray Tracing in One Weekend](https://raytracing.github.io/) de Peter Shirley.

## Éclairage direct de l'envmap (NEE + MIS)

Une HDRI réelle contient un soleil très lumineux mais minuscule : des rayons tirés au hasard le trouvent très rarement, et l'écrêtage (`max_radiance`) supprimait son énergie, ce qui donnait un nuage gris et des ombres floues.

- **Échantillonnage d'importance** de l'envmap : les directions sont tirées proportionnellement à la luminance de l'image (× sin θ), via des CDF marginale/conditionnelle.
- **Next-event estimation** : à chaque diffusion (dans le volume ou sur une surface diffuse), un rayon d'ombre est lancé vers une direction tirée dans l'envmap. La transmittance à travers les volumes est évaluée de façon stochastique.
- **Multiple importance sampling** (heuristique de puissance) pour combiner cet échantillonnage avec celui de la BRDF/fonction de phase, sans compter la lumière deux fois.
- Les rebonds Lambertiens sont échantillonnés en cosinus.

Limite : les matériaux spéculaires (métal, verre) ne reçoivent pas d'éclairage direct par NEE, ils passent par l'échantillonnage classique.
