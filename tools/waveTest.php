<?php

echo "SAW: ";
for ($phase=0; $phase < 32; $phase++) {
  $value = -8 + ($phase >> 1);
  printf("%01d ",$value);
}
echo "\n";

echo "TRI: ";
for ($phase=0; $phase < 32; $phase++) {
  if($phase < 8) {
    $value = $phase & 7;
  } else if ($phase < 24) {
    $value = 15 - $phase;
  } else {
    $value = -8 + ($phase & 7);
  }
  printf("%01d ",$value);
}
echo "\n";

echo "PUL: ";
$duty = 14;
for ($phase=0; $phase < 32; $phase++) {
  $value = ($phase >> 1 > $duty) ? 7 : -8;
  printf("%01d ",$value);
}
echo "\n";
