# Drush ddoto (aka. Drush drupal.org)

## Setup

Clone the repository in to your .drush folder and run `composer install`.

```
  mkdir ~/.drush
  git clone https://github.com/webflo/drush_ddoto.git ~/.drush/drush_ddoto
  cd ~/.drush/drush_ddoto
  composer install
```

Copy settings.example.php from vendor/klausi/github_drupalorg and provide the configuration settings.

```
  cp vendor/klausi/github_drupalorg/settings.example.php settings.php
  vim settings.php
```

