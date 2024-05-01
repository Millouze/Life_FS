## SCRIPTS

### benchmark.sh
Le script crée un répertoire dans `mnt` qui s'appelle `ouiche_fs` si il n'existe pas déjà, et mount l'image `test.img` dedans. De plus il insére le module `ouichefs` et lance le benchmark.

### img.sh

Le chemin du répetoire `share` est a modifier.
Le script compile le projet et le le repertoire `mkfs`, crée une image et déplaces les fichiers `test.img` et `ouichefs` dans le répertoire `share`.