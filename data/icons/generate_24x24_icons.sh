# we need to generate the 24x24 icons from the 22x22 tango icons
cd 22x22
for icon in *.png; do
	convert -bordercolor Transparent -border 1x1 $icon ../24x24/$icon;
done
cd -
