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
      'drush cbos publish /var/www/file.csv ' => 'Publish all the nodes on the file /var/www/file.csv',
      'drush cbos unpublish /var/www/file.csv ' => 'Unpublish all the nodes on the file /var/www/file.csv',
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
  $nid_list = cbo_drush_get_nids_from_csv($filepath);
  if (empty($nid_list)) {
    return drush_set_error(dt('No node to !action in given file.', array('!action' => $action)));
  }

  // Confirm delete.
  drush_print(dt("Found !count nodes to !action.", array('!count' => count($nid_list), '!action' => $action)));

  if (drush_confirm(dt('Do you want to !action them?', array('!action' => $action)))) {
    return _cbo_drush_update_content($nid_list, $action);
  }
}


/**
 * Parse CSV file to get node ids.
 *
 * @param $filepath
 * @return array
 */
function cbo_drush_get_nids_from_csv($filepath) {
  $nid_list = array();
  $lines = cbo_drush_read_csv($filepath);
  foreach ($lines as $line) {
    // We take the first element as URL/nid from the line.
    $path = $line[0];
    // Check if given value is nid. eg.1234
    if (is_numeric($path)) {
      $nid_list[] = $path;
    }
    // Check if given value is original node path e.g. node/1234
    elseif (arg(0, $path) == 'node' && is_numeric(arg(1, $path))) {
      $nid_list[] = arg(1, $path);
    }
    // Check if the given value is a path alias e.g. foo/bar
    elseif ($nid = cbo_drush_get_nid_from_path($path)) {
      $nid_list[] = $nid;
    }
  }
  return $nid_list;
}

/**
 * Helper to read CSV file.
 *
 * @param $filepath
 * @return array|bool|string
 */
function cbo_drush_read_csv($filepath) {
  $content = '';
  if (empty($filepath) || !file_exists($filepath)) {
    drush_log(dt('The file "%filepath" is not exists.', array('%filepath' => $filepath)), 'error');
    return FALSE;
  }

  // Automatically detect line endings.
  ini_set('auto_detect_line_endings', '1');
  $handle = fopen($filepath,'r');
  if ($handle) {
    // $title = fgetcsv($handle, 4096);
    while ($line = fgetcsv($handle, 4096)) {
      $content[] = $line;
    }
  }
  else {
    drush_log(dt('The file %filepath could not be opened.', array('%filepath' => $filepath)), 'error');
  }
  return $content;
}

/**
 * Performs update operations with batch set.
 *
 * @param array $nid_list
 * @param string $action
 */
function _cbo_drush_update_content($nid_list, $action) {
  // get the batch size from config.
  $batch_size = variable_get('cbo_batch_size', 1);

  // chunk the nids, instead of foreach.
  $batches = array_chunk($nid_list, $batch_size);

  $operations = array();
  foreach ($batches as $nids) {
    $operations[] = array('cbo_drush_update_content', array($nids, $action));
  }

  $batch = array(
    'operations' => $operations,
    'error_message' => t('Error while trying to !action your content. Please try again (BATCH_INIT_FAIL).', array('!action' => $action)),
    'finished' => 'cbo_drush_update_content_finished',
  );
  $batch['progressive'] = FALSE;
  batch_set($batch);
  drush_backend_batch_process();
}

/**
 * Performs update actions.
 *
 * @param $nid_list
 * @param string $action
 */
function cbo_drush_update_content($nid_list, $action = CBO_PUBLISH) {
  switch($action) {
    case CBO_PUBLISH:
    case CBO_UNPUBLISH:
      $status = $action == CBO_PUBLISH ? 1 : 0;
      $db_result = db_query('UPDATE {node} SET status=%d WHERE nid IN (' . db_placeholders($nid_list, 'int'). ')', array_merge(array($status), $nid_list));
      if (!$db_result) {
        drush_log('Error while updating', 'error');
      }
      break;
  }
}

/**
 * Callback for batch finish.
 */
function cbo_drush_update_content_finished() {
  drush_log('Updated successfully.', 'success');
}

/**
 * Helper to get node ID from path/alias.
 *
 * @param $path
 * @param null $path_language
 * @return null
 */
function cbo_drush_get_nid_from_path($path, $path_language = NULL) {
  global $language;
  $path_language = $path_language ? $path_language : $language->language;
  $src_path = db_result(db_query("SELECT src FROM {url_alias} WHERE dst = '%s' AND language IN('%s')", array($path, $path_language)));
  $path_parts = arg(NULL, $src_path);
  if ($path_parts[0] == 'node' && is_numeric($path_parts[1]) && count($path_parts) == 2) {
    return $path_parts[1];
  }
  return NULL;
}
