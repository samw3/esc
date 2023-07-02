<?
$step = 128;
$sampleFreq = 44100 * $step;

$c7 = 1046.5;

$q = pow(2.0, 1.0 / 12.0);

$freqs = array();
for ($i = 7; $i >= 0; $i--)
{
	for($n = 0; $n < 12; $n++)
	{
		$freqs[$i * 12 + $n] = $c7 * pow($q, $n) * pow(2, -$i);
	}
}

foreach( $freqs as $v)
{
	$waveStep = $v * 32;
	$period = (int)round($sampleFreq / $waveStep);
	$periods[] = $period;
	$roundedFreq = ($sampleFreq / $period / 32);
	$error = max($error,abs(((($sampleFreq / $period) - $waveStep) / $waveStep) * 100));
	echo "$roundedFreq $v $error \n";
}

?>
static const u16 waveStep[] =
{<?
$first = true;
$c = 0;
foreach( $periods as $v)
{
	if (( $c % 8) == 0) echo "\n\t";
	echo "0x" . str_pad(dechex($v), 4, "0", STR_PAD_LEFT) . ",";
	$c++;
}
?>

};
