<?php
/**
 *Implemenation of hook_cron.
 */

function parking_cron() {
  parking_send_reservation();
}
