<?php

global $drupal_user;
global $drupal_password;

use Drupal\github_drupalorg\IssuePriority;
use \Drupal\github_drupalorg\IssueStatus;

require_once __DIR__ . '/vendor/autoload.php';
require_once __DIR__ . '/settings.php';

/**
 * Implements hook_drush_command().
 */
function drush_ddoto_drush_command() {
  $items['post-comment'] = array(
    'description' => 'Post a comment on drupal.org',
    'options' => array(
      'issue_status' => array(
        'description' => 'Sets the issue status, such as needs review (8) or RTBC (14)',
        'example-value' => '14',
      ),
      'message' => array(
        'description' => 'The body of the comment.',
      ),
      'tags' => array(
        'description' => 'The tags of the issue.',
      ),
      'issue_priority' => array(
        'description' => 'The issue priority',
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
    'description' => 'Login to Drupal.org with the username specified in settings.php.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}

/**
 * Extract issue id.
 *
 * @param mixed $nid
 *
 * @return int
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
  $client->login((string) $drupal_password);
}

/**
 * Post comment on drupal.org
 */
function drush_drush_ddoto_post_comment($nid) {
  global $drupal_user;
  $interactive = TRUE;

  if ($content = drush_get_option('message')) {
    $interactive = FALSE;
    $issue_settings['content'] = $content;
  }

  $client = new \Drupal\github_drupalorg\Client($drupal_user);
  $issue_status_definition = IssueStatus::getDefinition();
  $issue_priority_definition = IssuePriority::getDefinition();
  $nid = _drush_ddoto_extract_issue_id($nid);
  $args = drush_get_arguments();
  // Drop the command name
  array_shift($args);
  // Drop the issue NID
  array_shift($args);
  $files = $args;

  // Fetch issue form.
  $form = $client->getForm($nid);
  $issue_settings['title'] = $form['title']->getValue();

  // Extract issue settings from edit form.
  $issue_settings['tags'] = $form['taxonomy_vocabulary_9[und]']->getValue();
  $issue_settings['status'] = $form['field_issue_status[und]']->getValue();
  $issue_settings['priority'] = $form['field_issue_priority[und]']->getValue();

  // Sane default for comments with file attachments.
  if ($files && !drush_get_option('issue_status')) {
    // Set the issue to "needs review" if it is not already "needs review" or RTBC.
    if ($issue_settings['status'] != IssueStatus::RTBC) {
      $issue_settings['status'] = IssueStatus::NEEDS_REVIEW;
    }
  }

  // Override issue settings with cli options.
  $issue_settings['tags'] = drush_get_option('tags', $issue_settings['tags']);
  $issue_settings['status'] = drush_get_option('issue_status', $issue_settings['status']);
  $issue_settings['priority'] = drush_get_option('issue_priority', $issue_settings['priority']);

  // Generate proper labels for status and priority.
  $issue_settings['status'] = isset($issue_status_definition[$issue_settings['status']]) ? $issue_status_definition[$issue_settings['status']]['label'] : $issue_settings['status'];
  $issue_settings['priority'] = isset($issue_priority_definition[$issue_settings['priority']]) ? $issue_priority_definition[$issue_settings['priority']]['label'] : $issue_settings['priority'];

  if ($interactive) {
    // Create tmp file
    $temp_file = drush_save_data_to_temp_file(_drush_ddoto_file_template($nid, $files, $issue_settings));

    $exec = drush_get_editor();
    $shell = drush_shell_exec_interactive($exec, $temp_file);
    if ($shell) {
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
      if (isset($file_info['priority'])) {
        $issue_settings['priority'] = $file_info['priority'];
      }
    }
  }

  $attachments = array();
  foreach ($files as $file) {
    $attachments[] = drush_is_absolute_path($file) ? $file : drush_cwd() . '/'. $file;
  }

  // Convert status value to integer.
  $issue_settings['status'] = IssueStatus::toValue($issue_settings['status']);
  $issue_settings['priority'] = IssuePriority::toValue($issue_settings['priority']);

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

  if (isset($issue_settings['priority'])) {
    $output[] = '#';
    $output[] = '# Priority: ' . $issue_settings['priority'];
    foreach (IssuePriority::getDefinition() as $definition) {
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
    if (strpos($line, '# Priority:') === 0) {
      $priority = trim(str_replace('# Priority: ', '', $line));

      $file_info['priority'] = $priority;
    }
  }

  return $file_info;
}

