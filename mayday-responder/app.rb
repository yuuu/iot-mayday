# require 'httparty'
require 'json'
require 'slack-ruby-client'

Slack.configure do |config|
  config.token = ENV['SLACK_TOKEN']
end

def lambda_handler(event:, context:)
  puts event

  { statusCode: 200, body: { message: "OK", }.to_json }
end
