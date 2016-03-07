<?php

/**
 * @file
 * Contains drush commands and implementations.
 */

/**
 * Implements hook_drush_command().
 */
function cbo_drush_command() {
  $items['cbo-status'] = array(
    'callback' => 'cbo_drush_change_status_from_csv',
    'description' => "Change the status of nodes in CSV file (by path or nid)",
    'required-arguments' => TRUE,
    'arguments' => array(
      'status' => 'Update the status to. Allowed values are publish and unpublish',
      'filepath' => 'The file to be processed. One path or nid per line',
    ),
    'examples' => array(
      'drush cbos publish /var/www/file.csv ' => 'Publish all the nodes in /var/www/file.csv file',
      'drush cbos unpublish /var/www/file.csv ' => 'Unpublish all the nodes in /var/www/file.csv file',
    ),
    'aliases' => array('cbos'),
  );

  $items['cbo-update-path'] = array(
    'callback' => 'cbo_update_path_from_csv',
    'description' => "Performs bulk operation on given nodes",
    'required-arguments' => TRUE,
    'arguments' => array(
      'filepath' => 'The file to be processed. One path record per line in "source,destination" format',
    ),
    'examples' => array(
      'drush cboupath /var/www/file.csv ' => 'Load nodes from the file at /var/www/file.csv to update path',
    ),
    'aliases' => array('cbopath'),
  );

  return $items;
}

/**
 * Updates status of node IDs from CSV.
 *
 *
 * @param $action
 *   Action to perform.
 *
 * @param string $filepath
 *   String filename.
 *
 * @return bool
 *
 */
function cbo_drush_change_status_from_csv($action, $filepath) {
  if (empty($filepath) || !file_exists($filepath)) {
    drush_log(dt('The file "%filepath" is not exists.', array('%filepath' => $filepath)), 'error');
    return FALSE;
  }

  $lines = cbo_read_csv($filepath);
  $list = cbo_get_nids_from_raw($lines);

  $nid_list = $list['nodes'];
  $error_list = $list['errors'];
  if (empty($nid_list) && empty($error_list)) {
    return drush_set_error(dt('No node to !action.', array('!action' => $action)));
  }


  if (!empty($error_list)) {
    // Write it to a file.
    $error_file = cbo_get_error_filename($filepath, $action);
    $fp = fopen($error_file, 'w+');
    fwrite($fp, implode("\n", $error_list));
    $replacements = array('!count' => count($nid_list), '!err_count' => count($list['errors']), '!action' => $action, '!error_file' => $error_file);
    drush_print(dt("Found !count nodes to !action and !err_count invalid nid/paths. Check !error_file for the errors.", $replacements));
  }
  else {
    drush_print(dt("Found !count nodes to !action.", array('!count' => count($nid_list), '!action' => $action)));
  }

  // Confirm delete.
  if (drush_confirm(dt('Do you want to !action them?', array('!action' => $action)))) {
    if ($operations = _cbo_get_status_update_content_batch_operations($nid_list, $action)) {
      $batch = array(
        'operations' => $operations,
        'error_message' => t('Error while trying to !action your content. Please try again (BATCH_INIT_FAIL).', array('!action' => $action)),
        'finished' => 'cbo_drush_update_content_finished',
      );
      $batch['progressive'] = FALSE;
      batch_set($batch);
      drush_backend_batch_process();
    }
  }
}

/**
 * Callback for batch finish.
 */
function cbo_drush_update_content_finished() {
  drush_log('Updated successfully.', 'success');
}
