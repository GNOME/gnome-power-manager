#!/bin/bash
#
# Tool to calculate the fdi entry in 10-video-power-policy for geeky people
# that know what vbetool and s3_mode are.
#
# Copyright 2006 Richard Hughes <richard@hughsie.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

tfile="/tmp/lshal.txt"
interface="org.freedesktop.Hal.Device.VideoAdapterPM"
fdifile="/usr/share/hal/fdi/policy/10osvendor/10-video-power-policy.fdi"
keypref="video_adapter_pm"
oldpid=""

convert_hex () {
	#converts decimal to hex
	printf "0x%X" $1
}

got_udi () {
	udi=$1
	# get info about this video adapter
	product=`hal-get-property --udi $udi --key pci.product`
	vendor=`hal-get-property --udi $udi --key pci.vendor`
	sub_product=`hal-get-property --udi $udi --key pci.subsys_product`
	sub_vendor=`hal-get-property --udi $udi --key pci.subsys_vendor`
	pid=`hal-get-property --udi $udi --key pci.product_id`
	pid=`convert_hex $pid`
	vid=`hal-get-property --udi $udi --key pci.vendor_id`
	vid=`convert_hex $vid`

	# subpid and subvid are almost at the dmi level anyway...
	subpid=`hal-get-property --udi $udi --key pci.subsys_product_id`
	subpid=`convert_hex $subpid`
	subvid=`hal-get-property --udi $udi --key pci.subsys_vendor_id`
	subvid=`convert_hex $subvid`

	# Some notebooks have 2 devices, both with the same pid - ignore the 2nd
	if [ "$oldpid" = "$pid" ]; then
		return
		fi
	oldpid=$pid

	# We only add the method if it's going to do something
	do_suspend_action=""
	do_resume_action=""
	echo "  <device>"
	echo "    <match key=\"pci.vendor_id\" int=\"$vid\"> <!-- $vendor -->"
	echo "      <match key=\"pci.product_id\" int=\"$pid\"> <!-- $product -->"
	echo "        <match key=\"pci.subsys_vendor_id\" int=\"$subvid\"> <!-- $sub_vendor -->"
	echo "          <match key=\"pci.subsys_product_id\" int=\"$subpid\"> <!-- $sub_product -->"
	if [ "$s3_bios" = "y" ]; then
		echo "            <merge key=\"$keypref.s3_bios\" type=\"bool\">true</merge>"
		do_suspend_action="true"
		fi
	if [ "$s3_mode" = "y" ]; then
		echo "            <merge key=\"$keypref.s3_mode\" type=\"bool\">true</merge>"
		do_suspend_action="true"
		fi
	if [ "$dpms_suspend" = "y" ]; then
		echo "            <merge key=\"$keypref.dpms_suspend\" type=\"bool\">true</merge>"
		do_suspend_action="true"
		fi
	if [ "$vga_mode_3" = "y" ]; then
		echo "            <merge key=\"$keypref.vga_mode_3\" type=\"bool\">true</merge>"
		do_resume_action="true"
		fi
	if [ "$dpms_on" = "y" ]; then
		echo "            <merge key=\"$keypref.dpms_on\" type=\"bool\">true</merge>"
		do_resume_action="true"
		fi
	if [ "$vbe_post" = "y" ]; then
		echo "            <merge key=\"$keypref.vbe_post\" type=\"bool\">true</merge>"
		do_resume_action="true"
		fi
	if [ "$vbestate_restore" = "y" ]; then
		echo "            <merge key=\"$keypref.vbestate_restore\" type=\"bool\">true</merge>"
		do_suspend_action="true"
		do_resume_action="true"
		fi
	if [ "$vbemode_restore" = "y" ]; then
		echo "            <merge key=\"$keypref.vbemode_restore\" type=\"bool\">true</merge>"
		do_suspend_action="true"
		do_resume_action="true"
		fi
	echo "            <append key=\"info.capabilities\" type=\"strlist\">video_adapter_pm</append>"
	echo "            <append key=\"info.interfaces\" type=\"strlist\">$interface</append>"
	if [ "$do_suspend_action" = "true" ]; then
		echo "            <append key=\"$interface.method_names\" type=\"strlist\">SuspendVideo</append>"
		echo "            <append key=\"$interface.method_signatures\" type=\"strlist\"></append>"
		echo "            <append key=\"$interface.method_execpaths\" type=\"strlist\">hal-system-video-suspend</append>"
		fi
	if [ "$do_resume_action" = "true" ]; then
		echo "            <append key=\"$interface.method_names\" type=\"strlist\">ResumeVideo</append>"
		echo "            <append key=\"$interface.method_signatures\" type=\"strlist\"></append>"
		echo "            <append key=\"$interface.method_execpaths\" type=\"strlist\">hal-system-video-resume</append>"
		fi
	echo "          </match>"
	echo "        </match>"
	echo "      </match>"
	echo "    </match>"
	echo "  </device>"
}

lshal > $tfile


echo "Here you will enter some information about what you already know about"
echo "suspending and resuming assuming you don't already have matched entries"
echo "for your videocard (check with 'lshal | grep video_adapter_pm')"
echo
echo "CVS HAL is required to use this tool"
echo
echo "Any information you don't know, just answer 'n' or press return"
echo
echo "Section 1/3 : Kernel parameters"
echo -n "Use S3_BIOS (y|n): " && read -e s3_bios
echo -n "Use S3_MODE (y|n): " && read -e s3_mode
echo
echo "Section 2/3 : Suspend actions"
echo -n "Use DPMS to force the screen off (y|n): " && read -e dpms_suspend
echo -n "Use vbestate restore (y|n): " && read -e vbestate_restore
echo -n "Use vbemode restore (y|n): " && read -e vbemode_restore
echo
echo "Section 2/3 : Resume actions"
echo -n "Set VGA text mode to mode 3 (y|n): " && read -e vga_mode_3
echo -n "Use DPMS to force the screen on (y|n): " && read -e dpms_on
echo -n "Use VGA vbe post (y|n): " && read -e vbe_post
echo
echo "The generated output should be copied into this file :"
echo "$fdifile"
echo "which may or may not exist on your system."
echo

echo "---------------- CUT HERE --------------------"

# Print some info so we can work out broken BIOS's or naughty OEM's
computer="/org/freedesktop/Hal/devices/computer"
system_manufacturer=`hal-get-property --udi $computer --key smbios.system.manufacturer`
system_product=`hal-get-property --udi $computer --key smbios.system.product`
bios_date=`hal-get-property --udi $computer --key smbios.bios.release_date`
bios_version=`hal-get-property --udi $computer --key smbios.bios.version`

echo "  <!--"
echo "    system.manufacturer: $system_manufacturer"
echo "    system.product:      $system_product"
echo "    bios.release_date:   $bios_date"
echo "    bios.version:        $bios_version"
echo "  -->"

#do this because hal-find-by-property can't handle integers...
while read key equals value valuetype
do
	# find the UDI of pci.device_class = 3 (video adapter)
	if [ "$key" = "udi" ]; then
		#removes single quotes
		udi=`echo "$value" | awk '{$0.gsub("\047","");print $0;}'`
	fi
	if [ "$key" = "pci.device_class" ]; then
		class="$value"
	fi
	if [ "$key" = "" ]; then
		if [ "$class" = "3" ]; then
			got_udi $udi
		fi
		udi=""
		class=""
	fi
done < $tfile
rm $tfile

echo "---------------- CUT HERE --------------------"
