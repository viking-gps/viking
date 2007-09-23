#!/usr/bin/perl

%states = ( "AL" => "61087", "AK" => "61095", "AZ" => "61081",
"AR" => "61091", "CA" => "61069", "CO" => "61076", "CT" => "61063",
"DE" => "61073", "DC" => "61072", "FL" => "61093", "GA" => "61089",
"HI" => "61094", "ID" => "61053", "IL" => "61071", "IL" => "61066",
"IA" => "61058", "KS" => "61078", "KY" => "61077", "LA" => "61092",
"ME" => "61048", "MD" => "61075", "MA" => "61059", "MI" => "61096",
"MN" => "61055", "MS" => "61088", "MO" => "61080", "MT" => "61047",
"NB" => "61060", "NV" => "61067", "NH" => "61057", "NJ" => "61065",
"NM" => "61086", "NY" => "61061", "NC" => "61083", "ND" => "61049",
"OH" => "61070", "OK" => "61082", "OR" => "61056", "PA" => "61062",
"RI" => "61064", "SC" => "61090", "DS" => "61050", "TN" => "61084",
"TX" => "61085", "UT" => "61068", "VT" => "61054", "VA" => "61079",
"WA" => "61046", "WV" => "61074", "WI" => "61052", "WY" => "61051" );

use FindBin qw($Bin);
chdir($Bin);

if ( $ARGV[2] ) { $BASEDIR = $ARGV[2]; $BASEDIR =~ s/([^A-Z0-9])/\\$1/g; }
else { $BASEDIR = "~/.viking-maps/dem24k" }

$tmpfile=`tempfile`;
chop $tmpfile;

# floor
if ($lat<0) {$lat-=(1/8);}
if ($lon<0) {$lon-=(1/8);}
$lat=int($ARGV[0]*8)/8;
$lon=int($ARGV[1]*8)/8;
$format = sprintf("%.3f,%.3f", $lat,$lon);
$line = `grep $format dem24k.dat`;
chop($line);
($bla, $state, $quad, $county) = split("\\|", $line);

print "$format $quad $state $county\n";

if (!$quad) { die "couldn't find correct quad" }
if ( -f "$state/$county/$quad.dem" ) {
  die "quad already exists";
}

$ilat = int($lat);
$ilon = int($lon);

system("mkdir -p $BASEDIR/$ilat/$ilon/");
system("touch $BASEDIR/$ilat/$ilon/$format.dem");

use WWW::Mechanize;

$agent = WWW::Mechanize->new();

$agent->agent_alias( 'Windows IE 6' );

$agent->get("https://secure.geocomm.com/accounts/login.php?type=");
$agent->field("username", "onelongpause");
$agent->field("password", "andyoubegin");
$agent->click();
$agent->follow("continue");
$agent->get("http://data.geocomm.com/catalog/US/".$states{$state}."/sublist.html");
$agent->follow($county);
$agent->follow("Digital Elevation Models");
open(COUNTY, ">$tmpfile");
print COUNTY $agent->content();
close(COUNTY);

# figure out form number
$tmp = "egrep \"<b>$quad, $state \\([0-9a-z]+\\)<\\/b>\" $tmpfile -n|cut -d: -f1";
$n = `$tmp`; #line num of <b>quad, ST</b>
chop($n);
$n = `head -n $n $tmpfile|grep "<form"|wc -l`;
chop($n);
$form = $n + 1;
print $form;
$agent->form($form);
$agent->click();

open(COUNTY, ">$tmpfile");
print COUNTY $agent->content();
close(COUNTY);

open(COUNTY,"$tmpfile");
while (<COUNTY>) {
  if (/(http:\/\/download.geocomm.com\/images\/.*?\/geocomm-w-80.gif)/) {
    system("wget --user-agent=\"Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1.3) Gecko/20061201 Firefox/2.0.0.3 (Ubuntu-feisty)\" $1 -O - > /dev/null ")
  }
  if (/(http:\/\/dl1.geocomm.com\/download\/.*?.DEM.SDTS.TAR.GZ)/) {
    $tarball = $1;
  }
}

system("rm $tmpfile");
$tmpfile = $tmpfile . ".tgz";
system("wget --user-agent=\"Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1.3) Gecko/20061201 Firefox/2.0.0.3 (Ubuntu-feisty)\" $tarball -O $tmpfile");

system("./repackage.sh $tmpfile $format");
system("mkdir -p $BASEDIR/$ilat/$ilon");

#TODO: make safe for all BASEDIR!!! escape!!!
system("rm $BASEDIR/$ilat/$ilon/$format.dem");
system("mv $format.dem $BASEDIR/$ilat/$ilon");

system("rm $tmpfile");
