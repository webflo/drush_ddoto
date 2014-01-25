<?php

global $drupal_user;
global $drupal_password;

use \Drupal\github_drupalorg\IssueStatus;

require_once __DIR__ . '/vendor/autoload.php';
require_once __DIR__ . '/settings.php';

/**
 * Implements hook_drush_command().
 */
function drush_ddoto_drush_command() {
  $items['post-comment'] = array(
    'description' => 'Post comment on drupal.org',
    'options' => array(
      'issue-status' => array(
        'description' => 'Sets the issue status, like needs review (8), needs RTBC (14)',
        'example-value' => '14',
      ),
    ),
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
  $issue_status = new IssueStatus();
  $issue_status_definition = $issue_status->getDefinition();
  $nid = _drush_ddoto_extract_issue_id($nid);
  $files = func_get_args();
  array_shift($files);

  // Setup tags.
  $form = github_drupalorg_get_form($nid);
  $tags = $form['taxonomy_vocabulary_9[und]']->getValue();
  $status = $form['field_issue_status[und]']->getValue();
  $status_label = isset($issue_status_definition[$status]) ? $issue_status_definition[$status]['label'] : $issue_status;

  $issue_settings = array();
  $issue_settings['tags'] = $tags;
  $issue_settings['status'] = $status_label;

  // Create tmp file
  $filepath = drush_save_data_to_temp_file(_drush_ddoto_file_template($nid, $files, $issue_settings));

  $exec = drush_get_editor();
  $hshell = drush_shell_exec_interactive($exec, $filepath);
  if ($hshell) {
    $content = file_get_contents($filepath);
    $file_info = _drush_ddoto_parse_file($content);
    $content = _drush_ddoto_valid_lines($content);

    if (empty($content)) {
      drush_user_abort();
      return FALSE;
    }

    $filepath = array();
    foreach ($files as $file) {
       $filepath[] = drush_is_absolute_path($file) ? $file : drush_cwd() . '/'. $file;
    }

    // Extract the issue settings.
    $issue_settings = array();
    if (isset($file_info['tags'])) {
      $issue_settings['tags'] = $file_info['tags'];
    }
    if (isset($file_info['status'])) {
      $issue_settings['status'] = $file_info['status'];
    }

    post_comment($nid, $content, $filepath, $issue_settings);
  }
}



function  _drush_ddoto_file_template($nid, $files, $issue_settings = array()) {
  $output[] = '

';

  $output[] = '#########################';
  $output[] = '# Post patch to issues nr. #' . $nid;
  $output[] = '#';
  $output[] = '# Attached files';

  foreach ($files as $filename) {
    $output[] = '#  - ' . $filename;
  }

  $output[] = '#';

  if (isset($issue_settings['status'])) {
    $output[] = '# Status: ' . $issue_settings['status'];
    foreach (IssueStatus::getDefinition() as $definition) {
      $output[] = '#  - ' . $definition['label'] . ' - ' . implode(', ', $definition['aliases']);
    }
  }

  if (isset($issue_settings['tags'])) {
    $output[] = '# Tags: ' . $issue_settings['tags'];
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

/**
 * Parse the information stored in the ddoto file template.
 */
function _drush_ddoto_parse_file($content) {
  $file_info = array();
  foreach (explode(PHP_EOL, $content) as $line) {
    if (strpos($line, '# Tags:') === 0) {
      $file_info['tags'] = trim(str_replace('# Tags: ', '', $line));
    }
    if (strpos($line, '# Status:') === 0) {
      $status = trim(str_replace('# Status: ', '', $line));

      // Convert status value to integer.
      $issue_status = IssueStatus::toInteger($status);
      $file_info['status'] = $issue_status ? $issue_status : NULL;
    }
  }

  return $file_info;
}

