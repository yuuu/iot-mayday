# require 'httparty'
require 'json'
require 'slack-ruby-client'

Slack.configure do |config|
  config.token = ENV['SLACK_TOKEN']
end

def message(response, user_id)
  "<@#{user_id}> " + {
    OK: '話しましょう！',
    Later: '5分待ってください',
    NG: '後ほど連絡します'
  }[response.to_sym]
end

def lambda_handler(event:, context:)
  slack_client = Slack::Web::Client.new
  res = slack_client.chat_postMessage(
    channel: event['channel'],
    thread_ts: event['thread_ts'],
    text: message(event['response'], event['user_id'])
  )

  { statusCode: 200, body: { message: "OK", }.to_json }
end
