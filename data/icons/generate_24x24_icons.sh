# we need to generate the 24x24 icons from the 22x22 tango icons
subdirs="actions apps status"

for subdir in $subdirs; do
	cd 22x22/$subdir
	for icon in *.png; do
		convert -bordercolor Transparent -border 1x1 $icon ../24x24/$subdir/$icon;
	done
	cd -
done

