<?php
/**
 * @file
 */

function mailchimp_http_build_query($data_array) {
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

function mailchimp_get_list_info($partner_id = NULL) {
  $list_info = array();
  if ($partner_id == 2093) {
    $list_info["api_key"] = MC_SITE2_APIKEY;
    $list_info["list_id"] = MC_SITE2_MASTER_LIST;
  } else {
    $list_info["api_key"] = MC_SITE1_APIKEY;
    $list_info["list_id"] = MC_SITE1_MASTER_LIST;
  }

  return $list_info;
}