
subdirs="actions apps status"

mkdir 32x32
for subdir in $subdirs; do
	cd scalable/$subdir
	mkdir ../../32x32/$subdir
	for i in *.svg;do 
		inkscape --without-gui --export-png="../../32x32/$subdir/"$( echo $i | cut -d . -f -1 ).png --export-dpi=72 --export-background-opacity=0 --export-width=32 --export-height=32 "$i";done
	cd -
done
