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
      'issue_status' => array(
        'description' => 'Sets the issue status, like needs review (8), needs RTBC (14)',
        'example-value' => '14',
      ),
      'message' => array(
        'description' => 'The body of the comment.',
      ),
      'tags' => array(
        'description' => 'The tags of the issue.',
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

  $items['drupal-org-login'] = array(
    'description' => 'Login-in on Drupal.org with',
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
function drush_drush_ddoto_post_comment_validate($nid = NULL) {
  $nid = _drush_ddoto_extract_issue_id($nid);
  if (!$nid) {
    drush_set_error($nid . ' is no valid drupal.org issue id.');
  }
}

/**
 * Perform login on drupal.org
 */
function drush_drush_ddoto_drupal_org_login() {
  global $drupal_user;
  global $drupal_password;
  $client = new \Drupal\github_drupalorg\Client($drupal_user);
  $client->login($drupal_password);
}

/**
 * Post comment on drupal.org
 */
function drush_drush_ddoto_post_comment($nid) {
  global $drupal_user;

  $client = new \Drupal\github_drupalorg\Client($drupal_user);
  $issue_status = new IssueStatus();
  $issue_status_definition = $issue_status->getDefinition();
  $nid = _drush_ddoto_extract_issue_id($nid);
  $args = drush_get_arguments();
  // Drop the command name
  array_shift($args);
  // Drop the issue NID
  array_shift($args);
  $files = $args;

  // Setup tags.
  $form = $client->getForm($nid);
  $tags = $form['taxonomy_vocabulary_9[und]']->getValue();
  $status = $form['field_issue_status[und]']->getValue();
  $status_label = isset($issue_status_definition[$status]) ? $issue_status_definition[$status]['label'] : $issue_status;

  $issue_settings = array();
  $issue_settings['title'] = $form['title']->getValue();
  $issue_settings['tags'] = drush_get_option('tags', $tags);
  $issue_settings['status'] = drush_get_option('issue_status', $status_label);

  if ($content = drush_get_option('message')) {
    $issue_settings['content'] = $content;
  }
  else {
    // Create tmp file
    $temp_file = drush_save_data_to_temp_file(_drush_ddoto_file_template($nid, $files, $issue_settings));

    $exec = drush_get_editor();
    $hshell = drush_shell_exec_interactive($exec, $temp_file);
    if ($hshell) {
      $content = file_get_contents($temp_file);
      $file_info = _drush_ddoto_parse_file($content);
      $content = _drush_ddoto_valid_lines($content);
      $issue_settings['content'] = $content;

      if (empty($content)) {
        drush_user_abort();
        return FALSE;
      }

      // Extract the issue settings.
      $issue_settings = array();
      if (isset($file_info['tags'])) {
        $issue_settings['tags'] = $file_info['tags'];
      }
      if (isset($file_info['status'])) {
        $issue_settings['status'] = $file_info['status'];
      }
    }
  }

  $attachments = array();
  foreach ($files as $file) {
    $attachments[] = drush_is_absolute_path($file) ? $file : drush_cwd() . '/'. $file;
  }

  // Convert status value to integer.
  $issue_settings['status'] = IssueStatus::toInteger($issue_settings['status']);

  $client->postComment($nid, $content, $attachments, $issue_settings);
}



function  _drush_ddoto_file_template($nid, $files, $issue_settings = array()) {
  $output = array('', '');

  $output[] = "# Please enter the comment message for your changes. Lines starting";
  $output[] = "# with '#' will be ignored, and an empty message aborts the comment.";
  $output[] = '#';
  $output[] = '# Comment on issue #' . $nid . ': ' . $issue_settings['title'];

  if (!empty($files)) {
    $output[] = '#';
    $output[] = '# Attached files';
    foreach ($files as $filename) {
      $output[] = '#  - ' . $filename;
    }
  }

  if (isset($issue_settings['status'])) {
    $output[] = '#';
    $output[] = '# Status: ' . $issue_settings['status'];
    foreach (IssueStatus::getDefinition() as $definition) {
      $output[] = '#  - ' . $definition['label'] . ' - ' . implode(', ', $definition['aliases']);
    }
  }

  if (isset($issue_settings['tags'])) {
    $output[] = '#';
    $output[] = '# Tags: ' . $issue_settings['tags'];
  }


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

      $file_info['status'] = $status;
    }
  }

  return $file_info;
}

