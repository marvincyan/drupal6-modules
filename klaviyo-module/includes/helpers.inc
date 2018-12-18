<?php
/**
 * @file
 */


function klaviyo_http_build_query($data_array) {
  foreach ($data_array as $k=>$v) {
    if (is_array($v)) {
      foreach ($v as $s=>$t) {
        if (is_array($t)) {
          foreach ($t as $a=>$b) {
            $data[] = $k."[".$s."][".$a."]"."=".urlencode($b);
          }
        } else {
          $data[] = $k."[".$s."]"."=".urlencode($t);
        }
      }
    } else {
      $data[] = $k."=".urlencode($v);
    }
  }

  return implode("&", $data);
}