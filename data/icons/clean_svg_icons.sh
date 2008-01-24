# clean to reduce file size
subdirs="actions apps status"

for subdir in $subdirs; do
	cd scalable/$subdir
	for i in *.svg; do 
		inkscape -f "$i" --vacuum-defs --export-plain-svg "$i";
	done
	cd -
done

