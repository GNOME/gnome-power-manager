# Making a Release
For up-to-date, but more general instructions, check the [GNOME handbook](https://handbook.gnome.org/maintainers/making-a-release.html).

## Update News
To list changes since last tag, e.g. `48.0` or `43.rc`:
```bash
git log --format="%s" 48.0.. | grep -i -v trivial | grep -v Merge | uniq
```

Add any user visible changes into `../data/appdata/org.gnome.PowerStats.metainfo.xml.in`

Run `meson dist`, and correct any problems encountered.

Then stage and commit news changes:
```bash
git commit -a -m "GNOME Power Manager 48.0"
git push
```

Make sure that the CI pipeline in Gitlab for the push has succeded.

## Perform a Release
A release is published automatically by Gitlab CI when pushing a protected tag. The uploaded tarball name will be in the form `gnome-power-manager-$TAG`. Make sure that the tag name matches the latest meson project version on the branch you're pushing the tag to.

Once you have made the decision to release the current tip of the branch, create a signed tag and then push it:

```bash
git tag -s 48.0 -m "==== Version 48.0 ===="
# wait
git push --tags
```

Make sure that the "release" stage in the CI pipeline in Gitlab for the push has succeded, and that the tarball is published in the GNOME [sources website](https://download.gnome.org/sources/gnome-power-manager/).

## Post Release
Do a post-release version bump by updating `meson.build`.

```bash
git commit -a -m "trivial: Post release version bump"
git push
```

