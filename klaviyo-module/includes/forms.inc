<?php


function klaviyo_lot_alert() {
  $form['email'] = array(
    '#type' => 'textfield',
    '#size' => 70,
    '#maxlength' => 180,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Alert me on new product')
  );

  return $form;
}