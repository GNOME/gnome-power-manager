
subdirs="actions apps status"

for subdir in $subdirs; do
	cd scalable/$subdir
	for i in *.svg;do 
		inkscape --without-gui --export-png="../48x48/"$( echo $i | cut -d . -f -1 ).png --export-dpi=72 --export-background-opacity=0 --export-width=48 --export-height=48 "$i";done
	cd -
done
