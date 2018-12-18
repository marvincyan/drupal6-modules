<?php
/**
 * @file
 */

function parking_get_auth_token($company_id = NULL) {
  $url = P_BASE_URL . "/api/authentication";

  $header = array(
    'Authorization: ' . P_AUTHORIZATION,
    'X-ApplicationId: ' . P_APP_ID,
    'Content-Type: application/json',
  );

  $data = array(
    "Email" => "",
    "Password" => ""
  );

  if (!is_null($company_id)) {
    $data['CompanyId'] = $company_id;
  }

  $reply = post_request($url, json_encode($data), $header, FALSE, "OFF");
  $json = json_decode($reply, TRUE);

  if (isset($json['AuthorizationToken'])) {
    return $json['AuthorizationToken'];
  } else {
    watchdog('parking', 'Failed to Authorize.', array(), WATCHDOG_ERROR);
    email_developer('Parking API Error: Failed to Authorize.');
    return FALSE;
  }   
}

function parking_get_companies($authorization) {
  $url = P_BASE_URL . "/api/companies/companiesbylocation";
  $company_id = P_COMPANY_ID;
  
  $header = array(
    'Authorization: ' . $authorization,
    'X-ApplicationId: ' . P_APP_ID,
    'Content-Type: application/json',
  );

  $data = "locationCodeId=1";

  $reply = get_request($url, $data, $header, FALSE, "OFF");
  $companies = json_decode($reply, TRUE);

  if (!empty($companies)) {
    foreach($companies as $company) {
      if ($company['Description'] == 'Parking') {
        $company_id = $company['Id'];
      }
    }

    return $company_id;
  } else {
    watchdog('parking', 'Failed to obtain companies.', array(), WATCHDOG_ERROR);
    email_developer('Parking API Error: Failed to obtain companies.');
    return FALSE;
  }
}

function parking_get_channels($authorization, $parking_address, $parking_zip) {
  $url = P_BASE_URL . "/api/channels/bysource";
  preg_match('/^\d+/', $parking_address, $address_number);

  $header = array(
    'Authorization: ' . $authorization,
    'X-ApplicationId: ' . P_APP_ID,
    'Content-Type: application/json',
  );

  $reply = get_request($url, $data, $header, FALSE, "OFF");
  $channels = json_decode($reply, TRUE);

  foreach ($channels as $k=>$lot) {
    preg_match('/^\d+/', $lot['Addresses'][0]['Address1'], $address_matches);
    preg_match('/^\d+/', $lot['Addresses'][0]['PostalCode'], $zip_matches);

    if ( ($address_number[0] == $address_matches[0]) && ($parking_zip == $zip_matches[0]) ) {
      return $lot['ChannelId'];
      break;
    }
  }

  watchdog('parking', 'Failed to match channel.', array(), WATCHDOG_ERROR);
  email_developer('Parking API Error: Failed to match channel.');
  return FALSE;
}

function parking_create_quote($authorization, $channel_id, $reservation) {
  $url = P_BASE_URL . "/api/quotes/aggregator";

  $header = array(
    'Authorization: ' . $authorization,
    'X-ApplicationId: ' . P_APP_ID,
    'Content-Type: application/json',
  );

  $data = array(
    "ChannelId" => $channel_id,
    "ConsumerId" => NULL,
    "StartDate" => date('Y-m-d\TH:i:s', $reservation['startdate']),
    "EndDate" => date('Y-m-d\TH:i:s', $reservation['enddate']),
    "NumberOfLoyaltyPointsToUse" => 0,
    "AdHocItems" => array(
      array(
        "Description" => "Reservation # from Product.com: ".$reservation['reservation_id'], //add apk too
        "ItemType" => 5,
        "Rate" => $reservation['cost'],
        "OperatorType" => 1,
        "Quantity" => 1,
        "IsDiscount" => FALSE,
        "IsTaxable" => FALSE,
        "IsCredit" => TRUE
      )
    )
  );

  $reply = post_request($url, json_encode($data), $header, FALSE, "OFF");
  $quote = json_decode($reply, TRUE);
  
  if (isset($quote['QuoteId'])) {
    return $quote['QuoteId'];
  } else {
    watchdog('parking', 'Failed to obtain Quote ID.', array(), WATCHDOG_ERROR);
    email_developer('Parking API Error: Failed to obtain Quote ID.');
    return FALSE;
  } 
}

function parking_create_reservation($authorization, $quote_id, $reservation) {
  $url = P_BASE_URL . "/api/reservations";

  $header = array(
    'Authorization: ' . $authorization,
    'X-ApplicationId: ' . P_APP_ID,
    'Content-Type: application/json',
  );

  $data = array(
    "QuoteId" => $quote_id,
    "FirstName" => $reservation['firstname'],
    "LastName" => $reservation['lastname'],
    "Phone" => "1234567890",
    "Email" => "XXX@XXX.com",
    "OverrideEmailConf" => TRUE,
    "OptReservationContact" => FALSE,
    "OptNotification" => FALSE,
    "OptRewards" => FALSE,
    "OptCancelPolicy" => TRUE
  );

  $reply = post_request($url, json_encode($data), $header, FALSE, "OFF");
  $parking_id = json_decode($reply, TRUE);

  if (is_numeric($parking_id)) {
    $query = "UPDATE {parking_sync} SET parking_id = %d, status = 'SUCCESS', synced_on = NOW() WHERE reservation_id = %d AND status = 'PENDING'";
    db_query($query, array($parking_id, $reservation['reservation_id']));
    watchdog('parking', 'Sent reservation ' . $reservation['reservation_id'] . ' to Parking.<br>Reservation ID from PRS: ' . $parking_id, array(), WATCHDOG_INFO);
  } else {
    $query = "UPDATE {parking_sync} SET status = 'ERROR', message = '' WHERE reservation_id = %d AND status = 'PENDING'";
    db_query($query, array($reservation['reservation_id']));
    watchdog('parking', 'No ID returned from Parking API for Reservation ID: ' . $reservation['reservation_id'], array(), WATCHDOG_ERROR);
    email_developer('Parking API Error: No ID returned from Parking API', 'No ID returned from Parking API for Reservation ID: ' . $reservation['reservation_id']);
  }
}

function parking_delete_reservation($authorization, $parking_id, $reservation_id) {
  $url = P_BASE_URL . "/api/reservations/" . $parking_id;

  $header = array(
    'Authorization: ' . $authorization,
    'X-ApplicationId: ' . P_APP_ID,
    'Content-Type: application/json',
  );

  $reply = delete_request($url, '', $header, FALSE, "OFF");
  $results = json_decode($reply, TRUE);

  if ($results) {
    $query = "UPDATE {parking_sync} SET status = 'CANCELLED', cancelled_on = NOW() WHERE reservation_id = %d AND (status = 'CANCEL' OR status = 'UPDATE')";
    db_query($query, array($reservation_id));
    watchdog('parking', 'Cancelled reservation ' . $reservation_id . ' on Parking.<br>Parking ID from PRS: ' . $parking_id, array(), WATCHDOG_INFO);
    return TRUE;
  } else {
    watchdog('parking', 'Failed to cancel: ' . $reservation_id . ' on Parking.<br>Parking ID from PRS: ' . $parking_id, array(), WATCHDOG_ERROR);
    email_developer('Parking API Error: Failed to cancel.', 'Reservation ID: ' . $reservation_id . '<br>Parking ID from PRS: ' . $parking_id);
    return FALSE;
  }
}


function parking_insert_pending($reservation_id, $parking_id) {
  if (parking_check_parking($parking_id)) {
    $query = "INSERT INTO {parking_sync} SET reservation_id = %d";
    db_query($query, array($reservation_id));
    return TRUE;
  }

  return FALSE;
}

function parking_cancel_reservation($reservation_id) {
  $query = "UPDATE {parking_sync} SET status = 'CANCEL' WHERE reservation_id = %d AND status = 'SUCCESS'";
  db_query($query, array($reservation_id));
}

function parking_update_reservation($reservation_id) {
  $query = "UPDATE {parking_sync} SET status = 'UPDATE' WHERE reservation_id = %d AND status = 'SUCCESS'";
  db_query($query, array($reservation_id));
}

function parking_get_pending_reservations() {
  $query = "SELECT 
    aws.reservation_id, 
    ap.parking_address, 
    ap.parking_zip 
  FROM {parking_sync} aws 
  LEFT JOIN aap_parking_reservation apr ON aws.reservation_id = apr.reservation_id 
  LEFT JOIN aap_parking ap ON apr.parking_id = ap.parking_id 
  WHERE status = 'PENDING'
  AND ap.parking_id NOT IN(123,456,789)";

  $results = db_query($query);
  while ($pending = db_fetch_array($results) ){
    $pendings[] = $pending;
  }

  return $pendings;
}

function parking_get_pending_cancellations() {
  $query = "SELECT parking_id, reservation_id FROM {parking_sync} WHERE status = 'CANCEL'";

  $results = db_query($query);
  while ($cancellation = db_fetch_array($results) ){
    $cancellations[] = $cancellation;
  }

  return $cancellations;
}

function parking_get_pending_updates() {
  $query = "SELECT 
    aws.parking_id, 
    aws.reservation_id, 
    ap.parking_id,
    ap.parking_address, 
    ap.parking_zip 
  FROM {parking_sync} aws 
  LEFT JOIN {aap_parking_reservation} apr ON aws.reservation_id = apr.reservation_id 
  LEFT JOIN {aap_parking} ap ON apr.parking_id = ap.parking_id 
  WHERE aws.status = 'UPDATE'
  AND ap.parking_id NOT IN(123,46,789)";

  $results = db_query($query);
  while ($update = db_fetch_array($results) ){
    $updates[] = $update;
  }

  return $updates;
}

function parking_check_parking($parking_id) {
  $query = "SELECT nid FROM {node} WHERE title = 'Parking' AND type = 'brand'";
  $p_id = db_result(db_query($query));

  $query = "SELECT count(*) FROM {aap_outer_parking} WHERE parking_id = %d AND value like '%%%d%'";
  return db_result(db_query($query, array($parking_id, $p_id)));
}
