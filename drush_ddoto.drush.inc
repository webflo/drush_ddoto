<?php

global $drupal_user;
global $drupal_password;

require_once __DIR__ . '/settings.php';

/**
 * Implements hook_drush_command().
 */
function drush_ddoto_drush_command() {
  $items['post-comment'] = array(
    'description' => 'Post comment on drupal.org',
    'arguments' => array(
      'nid' => 'Drupal.org Node id',
    ),
    'aliases' => array(
      'pc', // post comment
      'pp', // post patch
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  return $items;
}

/**
 * Extract issue id.
 *
 * @param $nid
 */
function _drush_ddoto_extract_issue_id($nid) {
  preg_match('/[0-9]+/', $nid, $matches);
  if (empty($matches[0])) {
    return FALSE;
  }
  return $matches[0];
}

/**
 * Validate the nid.
 *
 * @param $nid
 */
function drush_drush_ddoto_post_comment_validate($nid) {
  $nid = _drush_ddoto_extract_issue_id($nid);
  if (!$nid) {
    drush_set_error($nid . ' is no valid drupal.org issue id.');
  }
}

/**
 *
 */
function drush_drush_ddoto_post_comment($nid) {
  $files = func_get_args();
  array_shift($files);

  // Create tmp file
  $filepath = drush_save_data_to_temp_file(_drush_ddoto_file_template($nid, $files));

  $exec = drush_get_editor();
  $status = drush_shell_exec_interactive($exec, $filepath);
  if ($status) {
    $content = file_get_contents($filepath);
    $content = _drush_ddoto_valid_lines($content);

    if (empty($content)) {
      drush_user_abort();
      return FALSE;
    }

    require_once __DIR__ . '/github_drupalorg/utility.php';

    $filepath = array();
    foreach ($files as $file) {
       $filepath[] = drush_cwd() . '/'. $file;
    }

    $nid = _drush_ddoto_extract_issue_id($nid);
    post_comment($nid, $content, $filepath);
  }
}



function  _drush_ddoto_file_template($nid, $files) {
  $output[] = '

';

  $output[] = '#########################';
  $output[] = '# Post patch to issues nr. ' . $nid;
  $output[] = '#';
  $output[] = '# Attached files';

  foreach ($files as $filename) {
    $output[] = '# - ' . $filename;
  }

  $output[] = '#';
  $output[] = '#';
  $output[] = '# Delete the file content to abort the process.';

  return implode("\n", $output);
}

function _drush_ddoto_valid_lines($content) {
  $array = explode("\n", $content);
  $lines = array_filter($array, function ($value) {
    return $value[0] != '#';
  });
  return implode("\n", $lines);
}
